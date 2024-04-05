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
#include "Ulpi.h"

gtry::scl::usb::Ulpi::Ulpi(std::string_view pinPrefix) :
	m_area("Ulpi", true),
	m_state(State::idle, RegisterSettings{.clock = m_io.clock})
{
	m_io.pin(pinPrefix);

	generate();
	m_sim = std::make_shared<UlpiSimulator>();
	m_sim->addSimulationProcess(m_io);

	m_area.leave();
}

gtry::Bit gtry::scl::usb::Ulpi::setup(Phy::OpMode mode)
{
	HCL_ASSERT_HINT(mode == Phy::OpMode::FullSpeedFunction, "no impl");

	enum class InitState
	{
		pullupReset,
		pulldownDisable,
		done
	};

	auto area = m_area.enter("setup");
	ClockScope cs{ m_io.clock };
	Reg<Enum<InitState>> state{ InitState::pullupReset };
	state.setName("state");

	IF(state.current() == InitState::pullupReset)
	{
		Bit ready = regWrite(
			(uint8_t)Ulpi::regFunctionControl,
			(Ulpi::opModeFullSpeed << Ulpi::regFuncXcvrSelect) |
			(1 << Ulpi::regFuncTermSelect) |
			(1 << Ulpi::regFuncReset) |
			(1 << Ulpi::regFuncSuspendM)
		);
		IF(ready)
			state = InitState::pulldownDisable;
	}

	IF(state.current() == InitState::pulldownDisable)
	{
		Bit ready = regWrite(
			(uint8_t)Ulpi::regOtgControl,
			0
		);
		IF(ready)
			state = InitState::done;
	}

	return state.current() == InitState::done;
}

gtry::Bit gtry::scl::usb::Ulpi::regWrite(UInt address, UInt data)
{
	auto area = m_area.enter();

	UInt fullAddr = 6_b;
	fullAddr = zext(address);
	m_regAddr = cat("b10", fullAddr); // write command code
	m_regData = data;

	Bit ready = m_state.current() == State::setRegData;
	IF(ready)
		m_regAddr = 0;
	return ready;
}

gtry::Bit gtry::scl::usb::Ulpi::regRead(UInt address, UInt& data)
{
	auto area = m_area.enter();

	UInt fullAddr = 6_b;
	fullAddr = zext(address);
	m_regAddr = cat("b11", fullAddr); // read command code
	data = m_io.dataIn; // valid in getRegData cycle

	Bit ready = m_state.current() == State::getRegData;
	IF(ready)
		m_regAddr = 0;
	return ready;
}

void gtry::scl::usb::Ulpi::generate()
{
	ClockScope clk{ m_io.clock };
	m_regAddr = reg(m_regAddr, 0);
	m_regData = reg(m_regData);
	HCL_NAMED(m_regAddr);
	HCL_NAMED(m_regData);
	m_state.setName("m_state");

	IF(m_state.current() == State::idle)
	{
		IF(m_regAddr.msb()) // reg vs. transmit bit
			m_state = State::waitDirOut;
	}

	generateRxStatus();
	generateRegFSM();
	generateRxStream();
	generateTxStream();
}

void gtry::scl::usb::Ulpi::generateRegFSM()
{
	IF(m_state.current() == State::waitDirOut)
	{
		IF(m_io.dir == '0')
			m_state = State::setRegAddress;
	}
	IF(m_state.current() == State::setRegAddress)
	{
		m_io.dataOut = m_regAddr;
		IF(m_io.nxt)
		{
			IF(m_regAddr[6] == '0') // write vs. read bit
				m_state = State::setRegData;
			ELSE
				m_state = State::waitDirIn;
		}
	}
	IF(m_state.current() == State::setRegData)
	{
		m_io.dataOut = m_regData;
		IF(m_io.nxt)
			m_state = State::stop;
	}
	IF(m_state.current() == State::stop)
	{
		m_io.stp = '1';
		m_state = State::idle;
	}
	IF(m_state.current() == State::waitDirIn)
	{
		IF(m_io.dir == '1')
			m_state = State::getRegData;
	}
	IF(m_state.current() == State::getRegData)
	{
		m_state = State::idle;
	}
}

void gtry::scl::usb::Ulpi::generateRxStatus()
{
	IF(m_io.dir == '1' & reg(m_io.dir) == '1' & m_io.nxt == '0')
	{
		m_status.lineState = m_io.dataIn(0, 2_b);

		// VbusState
		m_status.sessEnd = m_io.dataIn(2, 2_b) == 0;
		m_status.sessValid = m_io.dataIn(2, 2_b) == 2;
		m_status.vbusValid = m_io.dataIn(2, 2_b) == 3;

		// RxEvent
		m_status.rxActive = m_io.dataIn[4];
		m_status.rxError = m_io.dataIn[5];
		m_status.hostDisconnect = m_io.dataIn(4, 2_b) == 2;

		m_status.id = m_io.dataIn[6];
		m_status.altInt = m_io.dataIn[7];
	}
	m_status = gtry::reg(m_status);
}

void gtry::scl::usb::Ulpi::generateRxStream()
{
	Bit rxValid = m_io.dir == '1' & reg(m_io.dir) == '1' & m_io.nxt == '1';

	Bit inTransfer;
	inTransfer = reg(inTransfer, '0');

	m_rx.sop = inTransfer == '0' & rxValid == '1';
	m_rx.valid = rxValid;
	m_rx.data = m_io.dataIn;

	m_rx.error = '0';
	m_rx.eop = '0';
	
	IF(inTransfer == '1')
	{
		IF(m_io.dir == '0')
			m_rx.eop = '1'; // end due to bus direction change
		ELSE IF(m_io.nxt == '0' & m_io.dataIn[4] == '0')
			m_rx.eop = '1'; // end due to RxActive low
	}

	HCL_NAMED(m_rx);

	IF(rxValid)
		inTransfer = '1';
	IF(m_rx.eop)
		inTransfer = '0';

	m_rx.valid.resetValue('0');
	m_rx.eop.resetValue('0');
}

void gtry::scl::usb::Ulpi::generateTxStream()
{
	m_tx.ready = m_io.dir == '0' & reg(m_io.dir) == '0' & m_io.nxt == '1';
	
	IF(m_tx.valid)
		m_io.dataOut = m_tx.data;

	Bit txTransfer;
	txTransfer = reg(txTransfer, '0');
	HCL_NAMED(txTransfer);

	IF(txTransfer == '0' & m_tx.valid == '1')
	{
		// sop
		m_io.dataOut.upper(4_b) = "b0100"; // TX CMD Transmit

		IF(m_tx.ready)
			txTransfer = '1';
	}

	IF(txTransfer == '1' & m_tx.valid == '0')
	{
		// eop
		m_io.stp = '1';
		txTransfer = '0';
	}
}

void gtry::scl::usb::UlpiIo::pin(std::string_view prefix)
{
	clock.setName(std::string(prefix) + "CLKIN");

	dataIn = tristatePin(dataOut, dataEn).setName(std::string(prefix) + "DATA");
	dir = pinIn().setName(std::string(prefix) + "DIR");
	nxt = pinIn().setName(std::string(prefix) + "NXT");
	pinOut(cs).setName(std::string(prefix) + "CS");
	pinOut(!reset).setName(std::string(prefix) + "RESET_n");
	pinOut(stp).setName(std::string(prefix) + "STP");

	// safe defaults
	reset = '0';
	cs = '1';
	stp = '0';
	dataEn = dir == '0' & reg(dir, {.clock = clock}) == '0';
	dataOut = 0;
}

gtry::scl::usb::UlpiSimulator::UlpiSimulator()
{
	m_register.fill(0);

	// incomplete list of default register values
	m_register[Ulpi::regFunctionControl] =
		(Ulpi::opModeFullSpeed << Ulpi::regFuncXcvrSelect) |
		(1 << Ulpi::regFuncSuspendM);

	m_register[Ulpi::regOtgControl] =
		(1 << Ulpi::regOtgDpPulldown) |
		(1 << Ulpi::regOtgDmPulldown);
}

void gtry::scl::usb::UlpiSimulator::addSimulationProcess(UlpiIo& io)
{
	auto clockPeriod = io.clock.absoluteFrequency();
	clockPeriod = hlim::ClockRational{
		clockPeriod.denominator(),
		clockPeriod.numerator()
	};

	DesignScope::get()->getCircuit().addSimulationProcess([=, me = this->shared_from_this()]()->SimProcess {
		simu(io.dir) = '0';
		simu(io.nxt) = '0';
		simu(io.dataIn) = 0;
		//co_await WaitFor(clockPeriod * hlim::ClockRational{1, 2});

		// setup rxStatus
		simu(io.dir) = '1';
		co_await WaitFor(clockPeriod);
		simu(io.dataIn) = 0xE; // VbusValid, data1 high
		co_await WaitFor(clockPeriod);
		simu(io.dir) = '0';
		simu(io.dataIn) = 0;

		std::mt19937 rng{ 18055 };

		while(1)
		{
			if(!m_sendQueue.empty())
			{
				std::vector<uint8_t> packet = m_sendQueue.front();
				m_sendQueue.pop();

				simu(io.dir) = '1';
				simu(io.nxt) = '1';
				co_await AfterClk(io.clock);
				for(uint8_t byte : packet)
				{
					simu(io.dataIn) = byte;
					co_await AfterClk(io.clock);
				}
				simu(io.dataIn) = 0;
				simu(io.dir) = '0';
				simu(io.nxt) = '0';
				co_await AfterClk(io.clock);
			}

			const uint64_t data = simu(io.dataIn);
			const uint64_t cmd = data >> 6;

			switch(cmd)
			{
			case 0: // NOOP
				HCL_ASSERT_HINT(data == 0, "bits 0:5 are reserved");
				break;

			case 1: // Transmit
				{
					co_await WaitFor(clockPeriod);
					std::vector<uint8_t> packet;

					while(simu(io.stp) == '0')
					{
						simu(io.nxt) = (rng() % 2) == 1;
						if(simu(io.nxt))
							packet.push_back((uint8_t)simu(io.dataIn));
						HCL_ASSERT_HINT(packet.size() <= 1024, "max usb packet length exceeded. stp beat missing?");
						co_await WaitFor(clockPeriod);
					}
					HCL_ASSERT(!packet.empty());
					if((packet.front() & 3) == 3)
					{
						HCL_ASSERT(packet.size() >= 2);
						packet.pop_back();
						packet.pop_back();
					}

					m_recvQueue.push(move(packet));
					simu(io.nxt) = false;
				}
				break;

			case 2: // RegWrite
				HCL_ASSERT_HINT(data != 0xAF, "no impl"); // extended address
				co_await WaitFor(clockPeriod);
				simu(io.nxt) = '1';
				co_await WaitFor(clockPeriod);
				writeRegister((uint8_t)data & 0x3F, (uint8_t)simu(io.dataIn));
				co_await WaitFor(clockPeriod);
				simu(io.nxt) = '0';
				HCL_ASSERT_HINT(simu(io.stp), "stop missing");
				break;

			case 3: // RegRead
				HCL_ASSERT_HINT(data != 0xEF, "no impl"); // extended address
				co_await WaitFor(clockPeriod);
				simu(io.nxt) = '1';
				co_await WaitFor(clockPeriod);
				simu(io.nxt) = '0';
				simu(io.dir) = '1';
				co_await WaitFor(clockPeriod);
				simu(io.dataIn) = readRegister((uint8_t)data & 0x3F);
				co_await WaitFor(clockPeriod);
				simu(io.dataIn).invalidate();
				simu(io.dir) = '0';
				break;

			default:;
			}

			co_await WaitFor(clockPeriod);
		}
	});

	DesignScope::get()->getCircuit().addSimulationProcess([=, me = this->shared_from_this()]()->SimProcess {

		co_await WaitFor(clockPeriod * hlim::ClockRational{50, 1});

		m_sendQueue.push({ 0xA5, 0x82, 0x31 }); // SOF
		m_sendQueue.push({ 0x2D, 0x00, 0x10 }); // SETUP
		m_sendQueue.push({ 0xC3, 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00, 0xDD, 0x94 }); // SETUP DATA0

		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT_HINT(popToken(TokenPid::ack), "ACK expected for get device descriptor setup packet");

		TokenPid dataPid = TokenPid::data1;
		for(size_t len : {9,9,3})
		{
			m_sendQueue.push({ 0x69, 0x00, 0x10 }); // IN

			while(m_recvQueue.empty())
				co_await AfterClk(io.clock);
			HCL_ASSERT(m_recvQueue.front().size() == len);
			HCL_ASSERT(popToken(dataPid));
			dataPid = dataPid == TokenPid::data0 ? TokenPid::data1 : TokenPid::data0;

			m_sendQueue.push({ 0xD2 }); // ACK
		}

		// status stage
		m_sendQueue.push({ 0xE1, 0x00, 0x10 }); // OUT
		m_sendQueue.push({ 0x4B, 0x00, 0x00 }); // empty DATA1
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));


		// set configuration (enable device)
		m_sendQueue.push({ 0x2D, 0x00, 0x10 }); // SETUP
		m_sendQueue.push({ 0xC3, 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x25 }); // SETUP DATA0
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));

		//	status stage
		m_sendQueue.push({ 0x69, 0x00, 0x10 }); // IN
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(m_recvQueue.front().size() == 1);
		m_recvQueue.pop();
		m_sendQueue.push({ 0xD2 }); // ACK

		// send data to endpoint 1 bad crc5
		m_sendQueue.push({ 0xE1, 0x80, 0xb0 }); // OUT
		m_sendQueue.push({ 0xC3, 0x31, 0x81, 0x6B }); // DATA0
		co_await WaitFor(clockPeriod * hlim::ClockRational{128, 1});
		HCL_ASSERT(m_recvQueue.empty());

		// send data to endpoint 1 bad crc16
		m_sendQueue.push({ 0xE1, 0x80, 0xa0 }); // OUT
		m_sendQueue.push({ 0xC3, 0x31, 0x32, 0x33, 0x34, 0x80, 0x6B }); // DATA0
		co_await WaitFor(clockPeriod* hlim::ClockRational{ 128, 1 });
		HCL_ASSERT(m_recvQueue.empty());

		// send data to endpoint 1
		m_sendQueue.push({ 0xE1, 0x80, 0xa0 }); // OUT
		m_sendQueue.push({ 0xC3, 0x31, 0x81, 0x6B }); // DATA0
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));

		// resend data to endpoint 1
		m_sendQueue.push({ 0xE1, 0x80, 0xa0 }); // OUT
		m_sendQueue.push({ 0xC3, 0x31, 0x81, 0x6B }); // DATA0
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));

		// send data to endpoint 1
		m_sendQueue.push({ 0xE1, 0x80, 0xa0 }); // OUT
		m_sendQueue.push({ 0x4B, 0x32, 0xC1, 0x6A }); // DATA1
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));

		// recv data from endpoint 1
		m_sendQueue.push({ 0x69, 0x80, 0xa0 }); // IN
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::data0));
		m_sendQueue.push({ 0xD2 }); // ACK

		// set address
		m_sendQueue.push({ 0x2D, 0x00, 0x10 }); // SETUP
		m_sendQueue.push({ 0xC3, 0x00, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEA, 0xA1 }); // SETUP DATA0
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(popToken(TokenPid::ack));

		//	status stage
		while(m_recvQueue.empty())
			co_await AfterClk(io.clock);
		HCL_ASSERT(m_recvQueue.front().size() == 1);
		m_recvQueue.pop();
		m_sendQueue.push({ 0xD2 }); // ACK

		// check address set
		m_sendQueue.push({ 0x2D, 0x00, 0x10 }); // SETUP

	});
}

void gtry::scl::usb::UlpiSimulator::writeRegister(uint8_t address, uint8_t value)
{
	uint8_t baseAddress = regBaseAddress(address);
	switch(address - baseAddress)
	{
	case 0: // write
		m_register[baseAddress] = value;
		break;
	case 1: // set
		m_register[baseAddress] |= value;
		break;
	case 2: // clear
		m_register[baseAddress] &= ~value;
		break;
	default:
		HCL_ASSERT(!"never");
	}
}

uint8_t gtry::scl::usb::UlpiSimulator::readRegister(uint8_t address) const
{
	uint8_t realAddress = regBaseAddress(address);
	return m_register[realAddress];
}

uint8_t gtry::scl::usb::UlpiSimulator::regBaseAddress(uint8_t address)
{
	// some registers have seperate wr, set and clear addresses
	if(address >= Ulpi::regFunctionControl && address < Ulpi::regUsbInterruptStatus)
	{
		address = (address - 4) / 3 * 3 + 4;
	}
	if(address >= Ulpi::regScratch && address < 0x1C)
	{
		address = (address - 0x16) / 3 * 3 + 0x16;
	}
	return address;
}

bool gtry::scl::usb::UlpiSimulator::popToken(TokenPid pid)
{
	bool ret = (m_recvQueue.front().front() & 0xF) == (uint8_t)pid;
	m_recvQueue.pop();
	return ret;
}
