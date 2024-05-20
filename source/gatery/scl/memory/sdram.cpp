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
#include "gatery/scl_pch.h"
#include "sdram.h"

#include "../Counter.h"
#include "../ShiftReg.h"
#include "../io/ddr.h"
#include "../stream/StreamArbiter.h"
#include "../stream/utils.h"

using namespace gtry::scl::sdram;

void Controller::generate(TileLinkUB& link)
{
	HCL_DESIGNCHECK_HINT(link.a->size.width() == BitWidth::last(m_burstLimit), "size width must match burst limit");

	auto scope = m_area.enter();
	m_sourceW = link.a->source.width();
	initMember();

	UInt transferLogSize = link.a->size.width();
	UInt transferSize = transferLengthFromLogSize(transferLogSize, m_dataBusWidth.bits() / 8);
	m_timer->generate(m_timing, m_cmdBus, transferSize, 1ull << m_burstLimit);

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
	auto maintenanceStream = strm::regDownstream(move(maintenanceArbiter.out()));
	cmdArbiter.attach(maintenanceStream);


	m_bankState = gtry::reg(m_bankState);
	HCL_NAMED(m_bankState);

	ready(link.a) = '0';
	for(size_t i = 0; i < m_bankState.size(); ++i)
	{
		TileLinkChannelA aIn;
		downstream(aIn) = downstream(link.a);
		IF(link.a->address(m_mapping.bank) == i)
			ready(link.a) = ready(aIn);
		ELSE
			valid(aIn) = '0';

		TileLinkChannelA aInReg = strm::regReady(move(aIn));
		auto [cmd, data] = bankController(aInReg, m_bankState[i], ConstUInt(i, m_cmdBus.ba.width()));
		cmd->bank = ConstUInt(i, m_cmdBus.ba.width()); // optional optimization

		cmdArbiter.attach(cmd);
		outArbiter.attach(data);
	}
	CommandStream& nextCommand = cmdArbiter.out();
	DataOutStream& nextData = outArbiter.out();
	cmdArbiter.generate();
	outArbiter.generate();

	HCL_NAMED(nextCommand);
	makeReadQueue(nextCommand);
	setResponse(*link.d);
	setMaskForRead(nextData);
	HCL_NAMED(nextData);
	driveCommand(nextCommand, nextData);
	transferLogSize = nextCommand->size;
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

	IF(!m_timer)
		m_timer = std::make_shared<SdramTimer>();
}

std::tuple<Controller::CommandStream, Controller::DataOutStream> Controller::bankController(TileLinkChannelA& link, BankState& state, UInt bank) const
{
	auto scope = Area("bankController").enter();
	HCL_NAMED(bank);
	HCL_NAMED(link);

	// command stream
	CommandStream cmd = translateCommand(state, link);
	setName(cmd, "bankCommand");
	IF(transfer(cmd))
		state = updateState(*cmd, state);

	CommandStream timedCmd = enforceTiming(cmd, bank);

	// write data stream
	DataOutStream data = translateCommandData(link);

	ready(link) = '0';
	IF(timedCmd->code == CommandCode::Write)
		ready(link) = ready(data);
	IF(timedCmd->code == CommandCode::Read)
		ready(link) = ready(timedCmd);

	Bit delayDataStream = '0';
	IF(sop(data))
	{
		IF(!transfer(timedCmd))
			delayDataStream = '1'; // bank not ready yet
		IF(timedCmd->code != CommandCode::Write & timedCmd->code != CommandCode::Read)
			delayDataStream = '1'; // not ready for CAS yet
	}
	ELSE
	{
		// we left command phase but link holds valid until write data has been transfered
		valid(timedCmd) = '0';
	}
	HCL_NAMED(delayDataStream);
	DataOutStream dataStalled = strm::stall(move(data), delayDataStream);

	setName(timedCmd, "outCmd");
	setName(dataStalled, "outData");
	return { std::move(timedCmd), std::move(dataStalled) };
}

size_t gtry::scl::sdram::Controller::writeToReadTiming() const
{
	if (m_timing.wr > m_timing.cl)
		return m_timing.wr - m_timing.cl;
	return 0;
}

size_t gtry::scl::sdram::Controller::readDelay() const
{
	size_t delay = m_timing.cl - 1;
	if (m_useOutputRegister)
		delay += 1;
	if (m_useInputRegister)
		delay += 1;
	return delay;
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

void gtry::scl::sdram::Controller::makeReadQueue(const CommandStream& cmd)
{
	auto ent = m_area.enter("readQueue");

	UInt numBeats = transferLengthFromLogSize(cmd->size, m_dataBusWidth.bits() / 8);
	HCL_NAMED(numBeats);

	ReadTask in{
		.size = cmd->size.width(),
		.source = cmd->source.width(),
		.beats = numBeats.width(),
	};
	HCL_NAMED(in);
	in.active.resetValue('0');
	m_readQueue.in(in);

	in.active = valid(cmd) & (cmd->code == CommandCode::Read | cmd->code == CommandCode::Write);
	in.read = cmd->code == CommandCode::Read;
	in.size = cmd->size;
	in.source = cmd->source;
	in.beats = numBeats;

	// resubmit for burst reads
	auto&& prev = m_readQueue[1];
	IF(prev.active & prev.read & prev.beats != 1)
	{
		in = prev;
		in.beats -= 1;
	}
}

Controller::CommandStream Controller::translateCommand(const BankState& state, const TileLinkChannelA& request) const
{
	auto scope = Area("translateCommand").enter();
	//HCL_NAMED(request);

	CommandStream cmd{ {
		.code = CommandCode::Precharge,
		.address = ConstBVec(m_addrBusWidth),
		.bank = request->address(m_mapping.bank),
		.size = request->size,
		.source = request->source,
	} };
	valid(cmd) = valid(request) & sop(request);

	IF(state.rowActive & state.activeRow != request->address(m_mapping.row))
	{
		cmd->code = CommandCode::Precharge;
		cmd->address[10] = '0';
	} ELSE IF(!state.rowActive)
	{
		cmd->code = CommandCode::Activate;
		cmd->address = zext((BVec)request->address(m_mapping.row));
	} ELSE
	{
		IF(request->opcode == (size_t)request->Get)
			cmd->code = CommandCode::Read;
		ELSE
			cmd->code = CommandCode::Write;
		cmd->address = zext((BVec)request->address(m_mapping.column));
	}
	
	HCL_NAMED(cmd);
	return cmd;
}

Controller::DataOutStream gtry::scl::sdram::Controller::translateCommandData(const TileLinkChannelA& request) const
{
	auto scope = Area("translateCommandData").enter();
	//HCL_NAMED(request);

	DataOutStream out{ request->data };

	Bit isWrite = request->opcode.upper(2_b) == 0;
	valid(out) = valid(request) & isWrite;
	byteEnable(out) = request->mask;
	eop(out) = eop(request);

	HCL_NAMED(out);
	return out;
}

Controller::CommandStream Controller::enforceTiming(CommandStream& cmd, UInt bank) const
{
	Bit cmdTimingValid = m_timer->can(cmd->code, bank);
	HCL_NAMED(cmdTimingValid);
	return scl::strm::stall(move(cmd), !cmdTimingValid);
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

	if(m_exportClockPin)
		pinOut(ddr('0', '1')).setName(prefix + "CLK");

	HCL_NAMED(bus);
	pinOut(bus.cke).setName(prefix + "CKE");
	pinOut(bus.csn).setName(prefix + "CSn");
	pinOut(bus.rasn).setName(prefix + "RASn");
	pinOut(bus.casn).setName(prefix + "CASn");
	pinOut(bus.wen).setName(prefix + "WEn");
	pinOut(bus.a).setName(prefix + "A");
	pinOut(bus.ba).setName(prefix + "BA");
	pinOut(bus.dqm).setName(prefix + "DQM");

	m_dataIn = *scl::sdram::moduleSimulation(bus);

	HCL_NAMED(outEnable);
	BVec m_dataInPin = (BVec)tristatePin(bus.dq, outEnable).setName(prefix + "DQ");
	m_dataIn.exportOverride(m_dataInPin);

	if (m_useInputRegister)
		m_dataIn = reg(m_dataIn);

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

	m_cmdBus.dqm = m_cmdBus.dqm.width().mask();
	m_cmdBus.dq = ConstBVec(m_cmdBus.dq.width());
	IF(valid(data))
	{
		m_cmdBus.dqm = ~byteEnable(data);
		m_cmdBus.dq = *data;
	}

	m_cmdBus.ba = (BVec)cmd->bank;
	m_cmdBus.a = cmd->address;

	ready(cmd) = '1';
	ready(data) = '1';
}

void gtry::scl::sdram::Controller::setMaskForRead(DataOutStream& data)
{
	HCL_DESIGNCHECK(m_timing.cl >= 2);
	auto&& task = m_readQueue[m_timing.cl - 2];
	IF(task.active & task.read)
	{
		sim_assert(valid(data) == '0') << "read write data bus conflict";

		valid(data) = '1';
		*data = ConstBVec(data->width());
		byteEnable(data) = byteEnable(data).width().mask();
		eop(data) = '1';
	}
}

void gtry::scl::sdram::Controller::setResponse(TileLinkChannelD& response)
{
	auto&& task = m_readQueue[readDelay()];
	valid(response) = task.active;
	response->opcode = (BVec)cat("2b00", task.read);
	response->param = 0;
	response->data = m_dataIn;
	response->size = zext(task.size);
	response->source = zext(task.source);
	response->sink = 0;
	response->error = '0';
}

Controller::BankState Controller::updateState(const Command& cmd, const BankState& state) const
{
	BankState newState = state;
	
	IF(cmd.code == CommandCode::Activate)
	{
		newState.rowActive = '1';
		newState.activeRow = cmd.address.lower(newState.activeRow.width());
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
			.size = ConstUInt(BitWidth::last(m_burstLimit)),
			.source = ConstUInt(m_sourceW),
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
		emrs,
		mrs,
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
		afterWaitState = InitState::emrs;
	}

	IF(state.current() == InitState::emrs)
	{
		cmd->bank = 1;
		cmd->code = CommandCode::ModeRegisterSet;
		cmd->address = 0;

		if(m_driveStrength == DriveStrength::Weak)
			cmd->address = 1 << 1;

		afterWaitState = InitState::mrs;
	}

	IF(state.current() == InitState::mrs)
	{
		cmd->code = CommandCode::ModeRegisterSet;
		cmd->address = m_burstLimit | (m_timing.cl << 4);

		afterWaitState = InitState::refresh1;
	}

	IF(state.current() != InitState::wait & transfer(cmd))
		state = InitState::wait;

	BitWidth rcCounterW = BitWidth::count(m_timing.rc);
	UInt refreshCounter = 3_b + rcCounterW;
	refreshCounter = reg(refreshCounter, 0);

	IF(state.current() == InitState::refresh1)
	{
		IF(refreshCounter.lower(rcCounterW) == 0)
			cmd->code = CommandCode::Refresh;
		refreshCounter += 1;
		IF(refreshCounter != 0)
			state = InitState::refresh1; // disable auto wait state

		afterWaitState = InitState::done;
	}

	IF(state.current() == InitState::done)
	{
		valid(cmd) = '0';
		state = InitState::done; // stay here forever
	}

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
	cmd->bank = 0; // is dont care but makes bank state multiplexing easier for timing module

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

struct BankState
{
	gtry::Bit rowActive;
	gtry::BVec activeRow;
};
BOOST_HANA_ADAPT_STRUCT(BankState, rowActive, activeRow);

gtry::scl::VStream<gtry::BVec> gtry::scl::sdram::moduleSimulation(const CommandBus& cmd, Standard standard)
{
	Area ent{ "scl_moduleSimulation", true };
	HCL_NAMED(cmd);

	BitWidth colAddrW = standard == Standard::sdram ? 8_b : 10_b;
	BitWidth addrWidth = cmd.ba.width() + cmd.a.width() + colAddrW;
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
	UInt writeDelay = "3b0";
	if (standard != Standard::sdram)
		writeDelay = modeCL - 1;
	HCL_NAMED(writeDelay);

	Vector<BankState> state(cmd.ba.width().count());
	for (auto& s : state)
	{
		s.activeRow = cmd.a.width();
		s.rowActive.resetValue('0');
	}
	state = gtry::reg(state);
	HCL_NAMED(state);

	UInt address = addrWidth;
	address = reg(address);
	BVec bank = cmd.ba.width();
	bank = reg(bank);
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
		BankState bankState = mux(cmd.ba, state);
		HCL_NAMED(bankState);

		IF(code == (size_t)CommandCode::Activate)
		{
			sim_assert(!bankState.rowActive) << "activate while not in idle state";
			bankState.rowActive = '1';
			bankState.activeRow = cmd.a;
		}
		IF(code == (size_t)CommandCode::Read)
		{
			sim_assert(bankState.rowActive) << "read in idle state";

			address = cat(cmd.ba, bankState.activeRow, cmd.a(0, colAddrW));
			bank = cmd.ba;
			bankState.rowActive = !cmd.a[10];

			writeBursts = 0;
			readBursts = ConstUInt(1, readBursts.width()) << modeBurstLength;
			if (standard == Standard::sdram)
			{
				IF(modeBurstLength == 7)
					readBursts = 256;
			}
			else
			{
				readBursts >>= 1;
			}
		}
		IF(code == (size_t)CommandCode::BurstStop)
		{
			readBursts = 0;
		}
		IF(code == (size_t)CommandCode::Precharge)
		{
			bankState.rowActive = '0';
			bankState.activeRow = ConstBVec(cmd.a.width());
		
			// PrefetchAll special case
			IF(cmd.a[10])
				for (auto& s : state)
					s.rowActive = '0';

			IF(cmd.a[10] | bank == cmd.ba)
			{
				readBursts = 0;
				writeBursts = 0;
			}
		}
		IF(code == (size_t)CommandCode::Write)
		{
			sim_assert(bankState.rowActive) << "write in idle state";

			address = cat(cmd.ba, bankState.activeRow, cmd.a(0, colAddrW));
			bank = cmd.ba;
			bankState.rowActive = !cmd.a[10];

			readBursts = 0;
			writeBursts = ConstUInt(1, writeBursts.width()) << modeBurstLength;
			if (standard == Standard::sdram)
			{
				IF(modeBurstLength == 7)
					writeBursts = 256;
				IF(modeWriteBurstLength)
					writeBursts = 1;
			}
			else
			{
				writeBursts >>= 1;
			}
		}
		IF(code == (size_t)CommandCode::ModeRegisterSet)
		{
			IF(cmd.ba == 0)
			{
				modeBurstLength = (UInt)cmd.a(0, 3_b);
				sim_assert(cmd.a[3] == '0') << "interleaved burst mode not implemented";
				modeCL = (UInt)cmd.a(4, 3_b);
				sim_assert(cmd.a[7] == '0') << "test mode is not allowed";

				if (standard == Standard::sdram)
					modeWriteBurstLength = cmd.a[9];
			}

			if (standard == Standard::sdram)
			{
				if(cmd.ba.width().bits() > 0)
					sim_assert(cmd.ba.upper(-1_b) == 0) << "unsupported MRS command";
				sim_assert(cmd.a.upper(cmd.a.width() - 10) == 0) << "reserved bits must be zero";
				//sim_assert(modeRegister(4, 3_b) != 0) << "zero CAS Latency not implemented";
			}
		}

		demux(cmd.ba, state, bankState);
	}
	HCL_NAMED(readBursts);
	HCL_NAMED(writeBursts);
	HCL_NAMED(address);


	// write to memory
	UInt writeAddr = delay(address, writeDelay);
	HCL_NAMED(writeAddr);
	
	IF(delay(writeBursts, writeDelay) != 0)
	{
		BVec writeData = storage[writeAddr];
		for (size_t i = 0; i < cmd.dqm.size(); ++i)
		{
			IF(!cmd.dqm[i])
				writeData(i * 8, 8_b) = cmd.dq(i * 8, 8_b);
		}
		HCL_NAMED(writeAddr);
		storage[writeAddr] = writeData;
	}

	// delay read data to simulate data bus
	ShiftReg readDelay{ std::tuple(storage[address].read(), readBursts != 0)};

	address(0, colAddrW) += 1;

	// drive output
	BVec out = ConstBVec(cmd.dq.width());
	auto [readData, readActive] = readDelay[modeCL - 1];
	HCL_NAMED(readActive);
	HCL_NAMED(readData);

	BVec readMask = reg(cmd.dqm);
	if(standard != Standard::sdram)
		readMask = 0;
	HCL_NAMED(readMask);

	IF(readActive & readMask != readMask.width().mask())
	{
		sim_assert(writeBursts == 0) << "data bus conflict";

		for (size_t i = 0; i < readMask.size(); ++i)
			IF(!readMask[i])
				out(i * 8, 8_b) = readData(i * 8, 8_b);
	}
	HCL_NAMED(out);
	return strm::createVStream(out, readActive);
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

Controller& gtry::scl::sdram::Controller::burstLimit(size_t logLimit)
{
	HCL_DESIGNCHECK_HINT(logLimit <= 3, "max burst for sdram is 2^3 = 8");
	m_burstLimit = logLimit;
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
	const auto clkRatioNs = memClock * hlim::ClockRational{ 1, 1'000'000'000 };
	const size_t clkNs = clkRatioNs.denominator() / clkRatioNs.numerator();
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

gtry::scl::sdram::Controller& gtry::scl::sdram::Controller::exportClockPin(bool enable)
{
	m_exportClockPin = enable;
	return *this;
}
