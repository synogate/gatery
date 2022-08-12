/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "sdram.h"

#include "../Counter.h"
#include "../stream/StreamArbiter.h"
#include "../stream/adaptWidth.h"

using namespace gtry::scl::sdram;

void Controller::generate(TileLinkUL& link)
{
	auto scope = m_area.enter();
	initMember();

	StreamArbiter<CommandStream> maintenanceArbiter;
	{
		auto initStream = initSequence();
		maintenanceArbiter.attach(initStream);

		auto refreshStream = refreshSequence(!valid(link.a));
		maintenanceArbiter.attach(refreshStream);

		maintenanceArbiter.generate();
	}

	StreamArbiter<DataOutStream> outArbiter;
	StreamArbiter<CommandStream> cmdArbiter;
	auto maintenanceStream = maintenanceArbiter.out().regDownstreamBlocking();
	cmdArbiter.attach(maintenanceStream);


	m_bankState = gtry::reg(m_bankState);
	HCL_NAMED(m_bankState);

	ready(link.a) = '0';
	for(size_t i = 0; i < m_bankState.size(); ++i)
	{
		TileLinkChannelA aIn;
		downstream(aIn) = downstream(link.a);
		IF(link.chanA().address(m_mapping.bank) == i)
			ready(link.a) = ready(aIn);
		ELSE
			valid(aIn) = '0';

		auto [cmd, data] = bankController(aIn, m_bankState[i]);
		cmd->bank = ConstUInt(i, m_cmdBus.ba.width()); // optional optimization

		cmdArbiter.attach(cmd);
		outArbiter.attach(data);
	}
	CommandStream& nextCommand = cmdArbiter.out();
	DataOutStream& nextData = outArbiter.out();
	cmdArbiter.generate();
	outArbiter.generate();

	//IF(transfer(nextCommand) & nextCommand->code == CommandCode::Activate)
	//	m_rasTimer = 0;
	//ELSE IF(m_rasTimer != m_rasTimer.width().last())
	//	m_rasTimer += 1;

	HCL_NAMED(nextCommand);
	HCL_NAMED(nextData);
	driveCommand(nextCommand, nextData);
}

void Controller::initMember()
{
	// setup parameter
	m_addrBusWidth = BitWidth{ std::max<uint64_t>(11, m_mapping.row.width) };

	// setup io
	m_cmdBus.a = m_addrBusWidth;
	m_cmdBus.ba = BitWidth{ (uint64_t)m_mapping.bank.width };
	m_cmdBus.dq = m_dataBusWidth;
	m_cmdBus.dqm = m_dataBusWidth / 8;
	makeBusPins(m_cmdBus, m_pinPrefix);
	makeBankState();

	m_rasTimer = BitWidth::count(m_timing.rc);

	Bit rasCommand = m_cmdBus.cke & !m_cmdBus.csn & !m_cmdBus.rasn & m_cmdBus.casn & m_cmdBus.wen;
	IF(rasCommand)
		m_rasTimer = 0;
	ELSE IF(m_rasTimer != m_rasTimer.width().last())
		m_rasTimer += 1;

	m_rasTimer = reg(m_rasTimer, 0);
	HCL_NAMED(m_rasTimer);
}

std::tuple<Controller::CommandStream, Controller::DataOutStream> Controller::bankController(TileLinkChannelA& link, BankState& state) const
{
	auto scope = Area("bankController").enter();

	Bit dataOutBusy;
	HCL_NAMED(dataOutBusy);

	// command stream
	CommandStream cmd = translateCommand(state, link);
	setName(cmd, "bankCommand");

	IF(transfer(cmd))
		state = updateState(*cmd, state);

	CommandStream timedCmd = enforceTiming(cmd);
	CommandStream stalledCmd = stall(timedCmd, dataOutBusy);


	// write data / read mask stream
	DataOutStream data = translateCommandData(link, dataOutBusy);

	Bit delayDataStream = '0';
	IF(sop(data))
	{
		IF(!valid(stalledCmd))
			delayDataStream = '1'; // bank not ready yet
		IF(stalledCmd->code != CommandCode::Write & stalledCmd->code != CommandCode::Read)
			delayDataStream = '1'; // not ready for CAS yet
	}
	ELSE
	{
		// we left command phase but link holds valid until write data has been transfered
		valid(stalledCmd) = '0';
	}
	HCL_NAMED(delayDataStream);
	DataOutStream dataStalled = stall(data, delayDataStream);

	setName(stalledCmd, "outCmd");
	setName(dataStalled, "outData");
	return { std::move(stalledCmd), std::move(dataStalled) };
}

size_t gtry::scl::sdram::Controller::writeToReadTiming() const
{
	if (m_timing.wr > m_timing.cl)
		return m_timing.wr - m_timing.cl;
	return 0;
}

void gtry::scl::sdram::Controller::makeBankState()
{
	m_bankState.resize(1ull << m_mapping.bank.width);
	for (BankState& state : m_bankState)
	{
		state.activeRow = BitWidth{ (uint64_t)m_mapping.row.width };
		state.rowActive.resetValue(false);
	}
}

void gtry::scl::sdram::Controller::makeWriteBurstAddress(CommandStream& stream)
{
	if (m_burstLimit == 0)
		return;

	UInt address = BitWidth{ m_burstLimit };

	IF(transfer(stream) & stream->code == CommandCode::Write)
		address += 1;
	IF(transfer(stream) & eop(stream))
		address = 0;
	address = reg(address, 0);
	setName(address, "writeBurstAddress");

	stream->address |= zext((BVec)address);
}

Controller::CommandStream Controller::translateCommand(const BankState& state, const TileLinkChannelA& request) const
{
	auto scope = Area("translateCommand").enter();
	HCL_NAMED(request);

	const TileLinkA& req = request.get<TileLinkA>();

	CommandStream cmd{ {
		.code = CommandCode::Precharge,
		.address = ConstBVec(m_addrBusWidth),
		.bank = req.address(m_mapping.bank)
	} };
	valid(cmd) = valid(request) & sop(request);

	IF(state.rowActive & state.activeRow != req.address(m_mapping.row))
	{
		cmd->code = CommandCode::Precharge;
	}
	ELSE IF(!state.rowActive)
	{
		cmd->code = CommandCode::Activate;
		cmd->address = zext((BVec)req.address(m_mapping.row));
	}
	ELSE
	{
		IF(req.opcode == (size_t)req.Get)
			cmd->code = CommandCode::Read;
		ELSE
			cmd->code = CommandCode::Write;
		cmd->address = zext((BVec)req.address(m_mapping.column));
	}
	
	HCL_NAMED(cmd);
	return cmd;
}

Controller::DataOutStream gtry::scl::sdram::Controller::translateCommandData(TileLinkChannelA& request, Bit& bankStall) const
{
	auto scope = Area("translateCommandData").enter();
	HCL_NAMED(request);

	DataOutStream out = request
		.add(Eop{ eop(request) })
		.reduceTo<RvPacketStream<BVec, ByteEnable>>();

	// insert read byte mask stream
	TileLinkA& req = request.get<TileLinkA>();
	UInt len = transferLengthFromLogSize(req.size, byteEnable(request).width().bits());
	HCL_NAMED(len);

	UInt readBeatsLeft = len.width();
	readBeatsLeft = reg(readBeatsLeft, 0);
	setName(readBeatsLeft, "readBeatsLeft_pre");

	enum class State {
		wait,
		readMask
	};
	Reg<Enum<State>> state{ State::wait };
	state.setName("state");

	IF(state.current() == State::wait)
	{
		IF(valid(request) & req.opcode == (size_t)TileLinkA::Get)
		{
			readBeatsLeft = len;
			state = State::readMask;
		}
	}
	HCL_NAMED(readBeatsLeft);

	IF(state.combinatorial() == State::readMask)
	{
		readBeatsLeft -= 1;

		valid(out) = '1';
		byteEnable(out) = (BVec)oext(1);
		*out = ConstBVec(out->width());
		eop(out) = '0';

		IF(readBeatsLeft == 0)
		{
			eop(out) = '1';
			state = State::wait;
		}
	}

	bankStall = '0';
	IF(state.current() == State::readMask)
	{
		ready(request) = '0';
		bankStall = '1';
	}

	HCL_NAMED(out);
	return out;
}

Controller::CommandStream Controller::enforceTiming(CommandStream& cmd) const
{
	auto scope = Area("scl_enforceTiming").enter();
	HCL_NAMED(cmd);

	Counter timer{
		std::max({
			m_timing.rcd, m_timing.ras, m_timing.rp
		})
	};
	IF(!timer.isLast())
		timer.inc();

	Counter rcTimer{ m_timing.rc };
	IF(!rcTimer.isLast())
		rcTimer.inc();

	Bit stall = '0';
	IF(cmd->code == CommandCode::Precharge)
	{
		IF(timer.value() < m_timing.ras - 1)
			stall = '1';

		IF(transfer(cmd))
			timer.reset();
	}
	ELSE IF(cmd->code == CommandCode::Activate)
	{
		IF(m_rasTimer < m_timing.rrd - 1)
			stall = '1';

		IF(timer.value() < m_timing.rp - 1)
			stall = '1';

		if (m_timing.ras + m_timing.rp < m_timing.rc)
			IF(!rcTimer.isLast())
				stall = '1';

		IF(transfer(cmd))
		{
			timer.reset();
			rcTimer.reset();
		}
	}
	ELSE
	{
		IF(timer.value() < m_timing.rcd - 1)
			stall = '1';
	}
	HCL_NAMED(stall);

	return scl::stall(cmd, stall);
}

void Controller::makeBusPins(const CommandBus& in, std::string prefix)
{
	Bit outEnable = m_dataOutEnable;
	CommandBus bus = in;
	if (m_useOutputRegister)
	{
		bus = gtry::reg(in);
		bus.cke = gtry::reg(in.cke, '0');
		bus.dqm = gtry::reg(in.dqm, ConstBVec(0, in.dqm.width()));
		outEnable = gtry::reg(outEnable, '0');
	}

	HCL_NAMED(bus);
	pinOut(bus.cke).setName(prefix + "CKE");
	pinOut(bus.csn).setName(prefix + "CSn");
	pinOut(bus.rasn).setName(prefix + "RASn");
	pinOut(bus.casn).setName(prefix + "CASn");
	pinOut(bus.wen).setName(prefix + "WEn");
	pinOut(bus.a).setName(prefix + "A");
	pinOut(bus.ba).setName(prefix + "BA");
	pinOut(bus.dqm).setName(prefix + "DQM");

	HCL_NAMED(outEnable);
	m_dataIn = (BVec)tristatePin(bus.dq, outEnable).setName(prefix + "DQ");
	HCL_NAMED(m_dataIn);
}

void Controller::driveCommand(CommandStream& cmd, DataOutStream& data)
{
	m_cmdBus.cke = '1';
	m_cmdBus.csn = !transfer(cmd);

	UInt cmdCode = cmd->code.numericalValue();
	m_cmdBus.rasn = !cmdCode[0];
	m_cmdBus.casn = !cmdCode[1];
	m_cmdBus.wen = !cmdCode[2];

	IF(transfer(data) & eop(data))
		m_dataOutEnable = '0';
	m_dataOutEnable = reg(m_dataOutEnable, '0');
	IF(transfer(cmd) & cmd->code == CommandCode::Write)
		m_dataOutEnable = '1';

	m_cmdBus.dqm = ConstBVec(0, m_cmdBus.dqm.width());
	m_cmdBus.dq = ConstBVec(m_cmdBus.dq.width());
	IF(valid(data))
	{
		m_cmdBus.dqm = byteEnable(data);
		m_cmdBus.dq = *data;
	}

	m_cmdBus.ba = (BVec)cmd->bank;
	m_cmdBus.a = cmd->address;

	ready(cmd) = '1';
	ready(data) = '1';
}

Controller::BankState Controller::updateState(const Command& cmd, const BankState& state) const
{
	BankState newState = state;
	
	IF(cmd.code == CommandCode::Activate)
	{
		newState.rowActive = '1';
		newState.activeRow = cmd.address;
	}
	
	IF(cmd.code == CommandCode::Precharge)
	{
		newState.rowActive = '0';
	}
	
	HCL_NAMED(newState);
	return newState;
}

Controller::CommandStream gtry::scl::sdram::Controller::makeCommandStream() const
{
	CommandStream out{ {
			.address = ConstBVec(m_addrBusWidth),
			.bank = ConstUInt(BitWidth{(unsigned)m_mapping.bank.width}),
	}};
	valid(out) = '0';
	return out;
}

Controller::CommandStream Controller::initSequence() const
{
	auto scope = m_area.enter("scl_initSequence");

	CommandStream cmd = makeCommandStream();
	cmd->code = CommandCode::Nop;
	cmd->bank = 0;
	valid(cmd) = '1'; // block other requests until init done

	enum class InitState {
		reset,
		wait,
		precharge,
		mrs,
		emrs,
		refresh1,
		done,
	};
	Reg<Enum<InitState>> state{ InitState::reset };
	state.setName("state");

	Enum<InitState> afterWaitState;
	afterWaitState = reg(afterWaitState);
	HCL_NAMED(afterWaitState);

	IF(state.current() == InitState::reset)
		afterWaitState = InitState::precharge;

	IF(state.current() == InitState::wait)
		state = afterWaitState;

	IF(state.current() == InitState::precharge)
	{
		cmd->code = CommandCode::Precharge;
		cmd->address = 1 << 10;
		afterWaitState = InitState::mrs;
	}

	IF(state.current() == InitState::mrs)
	{
		cmd->code = CommandCode::ModeRegisterSet;
		cmd->address = m_burstLimit | (m_timing.cl << 4);
		afterWaitState = InitState::emrs;
	}

	IF(state.current() == InitState::emrs)
	{
		cmd->bank = 1;
		cmd->code = CommandCode::ModeRegisterSet;
		cmd->address = 0;

		if(m_driveStrength == DriveStrength::Weak)
			cmd->address = 1 << 1;

		afterWaitState = InitState::refresh1;
	}

	IF(state.current() == InitState::refresh1)
	{
		cmd->code = CommandCode::Refresh;
		afterWaitState = InitState::done;
	}

	IF(state.current() == InitState::done)
	{
		valid(cmd) = '0';
	}

	IF(state.current() != InitState::wait & transfer(cmd))
		state = InitState::wait;

	HCL_NAMED(cmd);
	return cmd;
}

Controller::CommandStream gtry::scl::sdram::Controller::refreshSequence(const Bit& mayRefresh)
{
	auto scope = m_area.enter("scl_refreshSequence");

	CommandStream cmd = makeCommandStream();
	cmd->code = CommandCode::Nop;
	cmd->address = ConstBVec(m_addrBusWidth);
	cmd->address = 1 << 10; // All Banks

	enum class RefreshState
	{
		wait,
		preparePrecharge,
		precharge,
		precharging,
		refresh,
		refreshing,
		idle
	};
	Reg<Enum<RefreshState>> state{ RefreshState::wait };
	state.setName("state");

	Counter delayTimer{ std::max({m_timing.rp, m_timing.ras, m_timing.rc}) };
	IF(!delayTimer.isLast())
		delayTimer.inc();

	Counter timer{ m_timing.refi };
	timer.inc();

	IF(state.current() != RefreshState::wait &
		state.current() != RefreshState::idle)
	{
		// block command bus for entire refresh
		valid(cmd) = '1';
	}

	Bit canRefresh = timer.value() >= m_timing.refi / 4;
	Bit mustRefresh = timer.value() >= m_timing.refi * 7ul / 8;
	HCL_NAMED(canRefresh);
	HCL_NAMED(mustRefresh);

	IF(state.current() == RefreshState::wait)
	{
		delayTimer.reset();

		IF(mustRefresh | (canRefresh & mayRefresh))
		{
			Bit anyBankActive = '0';
			for (BankState& s : m_bankState)
				anyBankActive |= s.rowActive;

			IF(anyBankActive)
				state = RefreshState::preparePrecharge;
			ELSE
				state = RefreshState::refresh;
		}

		IF(mustRefresh)
			state = RefreshState::preparePrecharge;
	}

	IF(state.current() == RefreshState::preparePrecharge)
	{
		IF(delayTimer.value() >= m_timing.ras - 1)
			state = RefreshState::precharge;
	}

	IF(state.current() == RefreshState::precharge)
	{
		cmd->code = CommandCode::Precharge;
		delayTimer.reset();

		for (BankState& s : m_bankState)
			s.rowActive = '0';

		IF(transfer(cmd))
			state = RefreshState::precharging;
	}

	IF(state.current() == RefreshState::precharging)
	{
		IF(delayTimer.value() >= m_timing.rp - 1)
			state = RefreshState::refresh;
	}

	IF(state.current() == RefreshState::refresh)
	{
		cmd->code = CommandCode::Refresh;
		delayTimer.reset();

		IF(transfer(cmd))
			state = RefreshState::refreshing;
	}

	IF(state.current() == RefreshState::refreshing)
	{
		IF(delayTimer.isLast())
		{
			IF(mayRefresh & canRefresh)
			{
				state = RefreshState::refresh;
				timer.reset();
			}
			ELSE
			{
				state = RefreshState::idle;
			}
		}
	}

	IF(state.current() == RefreshState::idle)
	{
		IF(timer.isLast())
			state = RefreshState::wait;
	}

	HCL_NAMED(cmd);
	return cmd;
}

gtry::BVec gtry::scl::sdram::moduleSimulation(const CommandBus& cmd)
{
	auto scope = Area{ "scl_moduleSimulation" }.enter();

	BitWidth addrWidth = cmd.ba.width() + cmd.a.width() + 8_b;
	Memory<BVec> storage{ addrWidth.count(), cmd.dq.width() };
	storage.noConflicts();
	storage.setType(MemType::DONT_CARE, 0);
	storage.setName("storage");

	UInt modeBurstLength = 3_b;
	UInt modeCL = 3_b;
	Bit modeWriteBurstLength;
	modeBurstLength = reg(modeBurstLength, 3);
	HCL_NAMED(modeBurstLength);
	modeCL = reg(modeCL, 2);
	HCL_NAMED(modeCL);
	modeWriteBurstLength = reg(modeWriteBurstLength, '0');
	HCL_NAMED(modeWriteBurstLength);

	Vector<Controller::BankState> state(cmd.ba.width().count());
	for (auto& s : state)
	{
		s.activeRow = cmd.a.width();
		s.rowActive.resetValue('0');
	}
	state = gtry::reg(state);
	HCL_NAMED(state);

	UInt address = addrWidth;
	address = reg(address);
	UInt readBursts = 9_b;
	IF(readBursts != 0)
		readBursts -= 1;
	readBursts = reg(readBursts, 0);
	UInt writeBursts = 9_b;
	IF(writeBursts != 0)
		writeBursts -= 1;
	writeBursts = reg(writeBursts, 0);

	IF(cmd.cke & !cmd.csn)
	{
		UInt code = cat(!cmd.wen, !cmd.casn, !cmd.rasn);
		HCL_NAMED(code);
		
		IF(code == (size_t)CommandCode::Activate)
		{
			sim_assert(!mux(cmd.ba, state).rowActive) << "activate while not in idle state";
			demux(cmd.ba, state, {
				.rowActive = '1',
				.activeRow = cmd.a,
			});
		}
		IF(code == (size_t)CommandCode::Read)
		{
			sim_assert(mux(cmd.ba, state).rowActive) << "read in idle state";

			address = cat(cmd.ba, mux(cmd.ba, state).activeRow, cmd.a(0, 8_b));

			readBursts = ConstUInt(1, readBursts.width()) << modeBurstLength;
			IF(modeBurstLength == 7)
				readBursts = 256;

			IF(writeBursts > zext(modeCL - 1))
				writeBursts = zext(modeCL - 1);
		}
		IF(code == (size_t)CommandCode::BurstStop)
		{
			readBursts = 0;
		}
		IF(code == (size_t)CommandCode::Precharge)
		{
			const Controller::BankState blank{
				.rowActive = '0',
				.activeRow = ConstBVec(cmd.a.width()),
			};
			demux(cmd.ba, state, blank);
		
			// PrefetchAll special case
			IF(cmd.a[10])
				for (auto& s : state)
					s.rowActive = '0';
		}
		IF(code == (size_t)CommandCode::Write)
		{
			sim_assert(mux(cmd.ba, state).rowActive) << "write in idle state";

			address = cat(cmd.ba, mux(cmd.ba, state).activeRow, cmd.a(0, 8_b));

			writeBursts = ConstUInt(1, writeBursts.width()) << modeBurstLength;
			IF(modeBurstLength == 7)
				writeBursts = 256;
			IF(modeWriteBurstLength)
				writeBursts = 1;
		}
		IF(code == (size_t)CommandCode::ModeRegisterSet)
		{
			IF(cmd.ba == 0)
			{
				modeBurstLength = (UInt)cmd.a(0, 3_b);
				sim_assert(cmd.a[3] == '0') << "interleaved burst mode not implemented";
				modeCL = (UInt)cmd.a(4, 3_b);
				sim_assert(cmd.a(7, 2_b) == 0) << "test mode is not allowed";
				modeWriteBurstLength = cmd.a[9];
			}
			sim_assert(cmd.ba.msb() == '0') << "invalid MRS command";
			sim_assert(cmd.a.upper(cmd.a.width() - 10) == 0) << "reserved bits must be zero";
			//sim_assert(modeRegister(4, 3_b) != 0) << "zero CAS Latency not implemented";
		}
	}
	HCL_NAMED(readBursts);
	HCL_NAMED(writeBursts);
	HCL_NAMED(address);

	// write to memory
	IF(writeBursts != 0)
	{
		BVec data = storage[address];
		for (size_t i = 0; i < cmd.dqm.size(); ++i)
		{
			IF(cmd.dqm[i])
				data(i * 8, 8_b) = cmd.dq(i * 8, 8_b);
		}
		setName(data, "writeData");
		storage[address] = data;
	}

	// delay read data to simulate data bus
	std::vector<BVec> readDataDelay(8);
	std::vector<BVec> readDataMaskDelay(8);
	std::vector<Bit> readActiveDelay(8);
	readDataDelay[0] = storage[address];
	readDataMaskDelay[0] = cmd.dqm;
	readActiveDelay[0] = readBursts != 0;
	for (size_t i = 1; i < readDataDelay.size(); ++i)
	{
		readDataDelay[i] = reg(readDataDelay[i - 1]);
		readDataMaskDelay[i] = reg(readDataMaskDelay[i - 1]);
		readActiveDelay[i] = reg(readActiveDelay[i - 1], '0');
	}
	address(0, 8_b) += 1;

	// drive output
	BVec out = ConstBVec(cmd.dq.width());
	IF(mux(modeCL, readActiveDelay))
	{
		sim_assert(writeBursts == 0) << "data bus conflict";
		BVec readData = mux(modeCL, readDataDelay);
		BVec readDataMask = mux(modeCL, readDataMaskDelay);
		HCL_NAMED(readData);
		HCL_NAMED(readDataMask);

		for (size_t i = 0; i < cmd.dqm.size(); ++i)
			IF(readDataMask[i])
				out(i * 8, 8_b) = readData(i * 8, 8_b);
	}
	HCL_NAMED(out);
	return out;
}

Controller& gtry::scl::sdram::Controller::timings(const Timings& timingsInNs)
{
	m_timing = timingsInNs.toCycles(ClockScope::getClk().absoluteFrequency());
	return *this;
}

Controller& gtry::scl::sdram::Controller::addressMap(const AddressMap& map)
{
	m_mapping = map;
	return *this;
}

Controller& gtry::scl::sdram::Controller::burstLimit(size_t limit)
{
	HCL_DESIGNCHECK_HINT(limit <= 3, "max burst for sdram is 2^3 = 8");
	m_burstLimit = limit;
	return *this;
}

Controller& gtry::scl::sdram::Controller::dataBusWidth(BitWidth width)
{
	m_dataBusWidth = width;
	return *this;
}

Controller& gtry::scl::sdram::Controller::pinPrefix(std::string prefix)
{
	m_pinPrefix = std::move(prefix);
	return *this;
}

gtry::scl::sdram::Timings gtry::scl::sdram::Timings::toCycles(hlim::ClockRational memClock) const
{
	const size_t clkNs = (memClock * hlim::ClockRational{ 1'000'000'000 }).numerator();
	auto conv = [=](size_t val) { 
		return uint16_t((val + clkNs - 1) / clkNs); 
	};

	return {
		.cl = cl,
		.rcd = conv(rcd),
		.ras = conv(ras),
		.rp = conv(rp),
		.rc = conv(rc),
		.rrd = conv(rrd),
		.refi = conv(refi),

		.wr = wr,
	};
}

Controller& gtry::scl::sdram::Controller::driveStrength(DriveStrength value)
{
	m_driveStrength = value;
	return *this;
}
