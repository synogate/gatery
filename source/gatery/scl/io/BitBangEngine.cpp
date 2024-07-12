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
#include "BitBangEngine.h"

using namespace gtry;
using namespace gtry::scl;

RvStream<BVec> BitBangEngine::generate(RvStream<BVec> command, size_t numIo)
{
	Area scope{ "scl_BitBangEngine", true };
	HCL_DESIGNCHECK(command->width() == 8_b);
	HCL_NAMED(command);
	m_io.resize(numIo);

	bool hasSerialEngine = false;
	hasSerialEngine |= m_io.size() > m_ioClkIndex;
	hasSerialEngine |= m_io.size() > m_ioMosiIndex;
	hasSerialEngine |= m_io.size() > m_ioMisoIndex;

	RvStream<BVec> out = strm::createVStream<BVec>(8_b, '0').add(Ready{});

	enum class CmdState {
		idle,
		invalid,
		write_out,
		write_en,
		load_low,	// currently used for open drain mode setup only
		load_high,
		inc_bits,
		inc_low,
		inc_high,
		clock_setup,
		clock_active,
		clock_wait,
		wait_for_gpio_signal,
	};
	Reg<Enum<CmdState>> state{ CmdState::idle };
	state.setName("state");
	Reg<Enum<CmdState>> followupState{ CmdState::idle };
	followupState.setName("followupState");

	ConfigState config;
	config.clockThreePhase.resetValue('0');
	config.dataLoopback.resetValue('0');
	config.idleClockState.resetValue('0');

	UInt bitLength = 2 * command->width() + 3_b + 1_b;
	UInt clockDiv = 2 * command->width() + 1_b;
	Bit tick = Counter{ clockDiv }.isLast();		HCL_NAMED(tick);

	ready(command) = '0';
	IF(state.current() == CmdState::idle & valid(command) & ready(out))
	{
		ready(command) = '1';
		followupState = CmdState::idle;
		bitLength = 0;

		config.clockDelay = '0';
		config.shiftIn = '0';
		config.shiftOut = '0';
		config.tmsOutMode = '0';
		config.stopClockOnSignal = '0';
		config.stopClockOnLastBit = '1';

		IF(command->msb() == '0')
		{
			if (hasSerialEngine)
			{
				config.clockDelay = (command->at(0) != config.idleClockState) & !config.clockThreePhase;

				config.captureEdge = command->at(0) != command->at(2);
				config.msbFirst = !command->at(3);
				config.shiftOut = command->at(4) | command->at(6);
				config.shiftIn = command->at(5);
				config.tmsOutMode = command->at(6);

				followupState = CmdState::clock_setup;
				state = CmdState::inc_low;
				IF(command->at(1))
					state = CmdState::inc_bits;
			}
		}
		ELSE IF((*command)(2, 5_b) == 0)
		{
			config.targetPinGroup = (UInt)(*command)(1, 1_b);

			IF(command->lsb())
			{
				*out = 0;
				for (size_t i = 0; i < m_io.size(); ++i)
					IF(i / 8 == config.targetPinGroup)
						(*out)[i % 8] = m_io[i].in;
				valid(out) = '1';
			}
			ELSE
			{
				state = CmdState::write_out;
			}
		}
		ELSE IF((*command)(1, 6_b) == 0x04 >> 1) // loopback mode
		{
			config.dataLoopback = !command->lsb();
		}
		ELSE IF(command->lower(7_b) == 0x6) // set clock div
		{
			state = CmdState::inc_low;
		}
		ELSE IF(command->lower(7_b) == 0x7) // flush buffer
		{
			// flush buffers (no need to implement in USB1.1)
		}
		ELSE IF((*command)(1, 6_b) == 0x08 >> 1) // wait for signal
		{
			config.stopClockSignal = !command->lsb();
			state = CmdState::wait_for_gpio_signal;
		}
		ELSE IF((*command)(1, 6_b) == 0x0C >> 1) // enable 3 phase clocking
		{
			config.clockThreePhase = !command->lsb();
		}
		ELSE IF((*command)(1, 6_b) == 0x0E >> 1) // clock only
		{
			if (hasSerialEngine)
			{
				followupState = CmdState::clock_setup;
				state = CmdState::inc_bits;
				IF(command->lsb())
					state = CmdState::inc_low; // byte mode
			}
		}
		ELSE IF((*command)(1, 6_b) == 0x14 >> 1) // clock until high/low
		{
			if (m_io.size() > m_ioStopClockIndex)
			{
				config.stopClockOnLastBit = '0';
				config.stopClockOnSignal = '1';
				config.stopClockSignal = !command->lsb();
				state = CmdState::clock_setup;
			}
		}
		ELSE IF((*command)(1, 6_b) == 0x1C >> 1) // clock until high/low with timeout
		{
			if (m_io.size() > m_ioStopClockIndex)
			{
				config.stopClockOnLastBit = '1';
				config.stopClockOnSignal = '1';
				config.stopClockSignal = !command->lsb();
				followupState = CmdState::clock_setup;
				state = CmdState::inc_low; // byte mode
			}
		}
		ELSE IF(command->lower(7_b) == 0x1E) // set open drain mode
		{
			state = CmdState::load_low;
		}
		ELSE IF((*command)(5, 2_b) == 2) // synogate fast bit bang mode
		{
			ready(command) = '0';
			IF(tick)
			{
				for (size_t i = 0; i < std::min<size_t>(4, m_io.size()); ++i)
					m_io[i].out = (*command)[i];

				*out = 0;
				for(size_t i = 0; i < std::min<size_t>(8, m_io.size()); ++i)
					out->at(i) = m_io[i].in;

				ready(command) = '1';
				valid(out) = command->at(4);
			}
		}
		ELSE
		{
			*out = 0xFA;
			valid(out) = '1';
			ready(command) = '0'; // we need to mirror the offending command byte
			state = CmdState::invalid;
		}
	}
	config = reg(config);
	HCL_NAMED(config);

	if (m_io.size() > std::max(m_ioMosiIndex, m_ioMisoIndex))
	{
		IF(config.dataLoopback)
			m_io[m_ioMisoIndex].in = m_io[m_ioMosiIndex].in;
	}

	IF(state.current() == CmdState::invalid & ready(out))
	{
		*out = *command;
		valid(out) = '1';
		ready(command) = '1';
		state = CmdState::idle;
	}

	IF(state.current() == CmdState::wait_for_gpio_signal)
	{
		if (m_io.size() > m_ioStopClockIndex)
			IF(m_io[m_ioStopClockIndex].in == config.stopClockSignal)
				state = CmdState::idle;
	}

	IF(state.current() == CmdState::write_out & valid(command))
	{
		for (size_t i = 0; i < m_io.size(); ++i)
			IF(i / 8 == config.targetPinGroup)
			{
				m_io[i].out = (*command)[i % 8];
				if(m_ioClkIndex == i)
					config.idleClockState = (*command)[i % 8];
			}

		ready(command) = '1';
		state = CmdState::write_en;
	}

	IF(state.current() == CmdState::write_en & valid(command))
	{
		for (size_t i = 0; i < m_io.size(); ++i)
			IF(i / 8 == config.targetPinGroup)
				m_io[i].en = (*command)[i % 8];
		ready(command) = '1';
		state = CmdState::idle;
	}

	Bit carryIn;
	carryIn = reg(carryIn, '1');
	HCL_NAMED(carryIn);
	UInt cmdInc = zext((UInt)*command, +1_b) + carryIn;
	HCL_NAMED(cmdInc);


	IF(state.current() == CmdState::inc_bits & valid(command))
	{
		bitLength.lower(cmdInc.width()) = cmdInc;
		ready(command) = '1';
		state = followupState.current();
	}

	IF(state.current() == CmdState::inc_low & valid(command))
	{
		IF(followupState.current() == CmdState::idle)
			clockDiv.lower(command->width()) = cmdInc.lower(-1_b);
		ELSE
			bitLength(3, command->width()) = cmdInc.lower(-1_b);

		ready(command) = '1';
		carryIn = cmdInc.msb();
		state = CmdState::inc_high;
	}

	IF(state.current() == CmdState::inc_high & valid(command))
	{
		IF(followupState.current() == CmdState::idle)
			clockDiv.upper(cmdInc.width()) = cmdInc;
		ELSE
			bitLength.upper(cmdInc.width()) = cmdInc;

		ready(command) = '1';
		carryIn = '1';
		state = followupState.current();
	}

	bitLength = reg(bitLength);						HCL_NAMED(bitLength);
	clockDiv = reg(clockDiv, 1);					HCL_NAMED(clockDiv);

	if (hasSerialEngine)
	{
		Bit setupEdge = '0';
		Bit captureEdge = '0';
		Bit toggleClock = '0';
		Bit toggleClockDelayed;
		toggleClockDelayed = reg(toggleClockDelayed, '0');

		Bit lastEdge = (config.stopClockOnLastBit & bitLength == 1);
		if (m_io.size() > m_ioStopClockIndex)
			lastEdge |= config.stopClockOnSignal & m_io[m_ioStopClockIndex].in == config.stopClockSignal;
		HCL_NAMED(lastEdge);

		IF(tick)
		{
			Io* clkIo = m_io.size() > m_ioClkIndex ? &m_io[m_ioClkIndex] : nullptr;
			Bit waitForData = config.shiftOut & !valid(command);		HCL_NAMED(waitForData);
			Bit waitForReady = config.shiftIn & !ready(out);			HCL_NAMED(waitForReady);

			IF(state.current() == CmdState::clock_setup & !waitForData & !waitForReady)
			{
				Bit clockDidToggle = clkIo ? (!clkIo->en | (clkIo->out == clkIo->in)) : Bit{ '1' };
				HCL_NAMED(clockDidToggle);
				IF(clockDidToggle)
				{
					setupEdge = config.shiftOut;
					captureEdge = config.shiftIn & !config.captureEdge;
					state = CmdState::clock_wait;
					IF(!config.clockThreePhase)
					{
						toggleClock = '1';
						state = CmdState::clock_active;
					}
				}
			}

			IF(state.current() == CmdState::clock_active & !waitForReady)
			{
				captureEdge = config.shiftIn & config.captureEdge;
				toggleClock = '1';
				state = CmdState::clock_setup;

				bitLength -= 1;
				IF(lastEdge)
					state = CmdState::idle;
			}

			IF(state.current() == CmdState::clock_wait)
			{
				toggleClock = '1';
				state = CmdState::clock_active;
			}

			HCL_NAMED(toggleClock);
			HCL_NAMED(toggleClockDelayed);
			if (clkIo)
				clkIo->out ^= mux(config.clockDelay, { toggleClock, toggleClockDelayed });
			toggleClockDelayed = toggleClock;
		}

		HCL_NAMED(setupEdge);
		HCL_NAMED(captureEdge);

		UInt lastBitIndex = 7;
		IF(config.tmsOutMode)
			lastBitIndex = 6;
		HCL_NAMED(lastBitIndex);

		if (m_io.size() > m_ioMosiIndex)
		{
			IF(setupEdge)
			{
				Counter bitPosCounterOut(8);
				IF(lastEdge | bitPosCounterOut.value() == lastBitIndex)
				{
					ready(command) = '1';
					bitPosCounterOut.reset();
				}
				UInt outBitIndex = (bitPosCounterOut.value() + (config.tmsOutMode & config.msbFirst)) ^ config.msbFirst;
				HCL_NAMED(outBitIndex);

				Bit outBit = mux(outBitIndex, *command);
				m_io[m_ioMosiIndex].out = outBit;
				
				IF(config.tmsOutMode)
				{
					m_io[m_ioMosiIndex].out = command->msb();
					if (m_io.size() > m_ioTmsIndex)
						m_io[m_ioTmsIndex].out = outBit;
				}
			}
		}

		if(m_io.size() > m_ioMisoIndex)
		{
			UInt captureBuffer = 8_b;
			captureBuffer = reg(captureBuffer, 0);
			HCL_NAMED(captureBuffer);

			IF(captureEdge)
			{
				Counter bitPosCounterIn(8);

				IF(config.msbFirst)
					captureBuffer = cat(captureBuffer.lower(-1_b), m_io[m_ioMisoIndex].in);
				ELSE
					captureBuffer = cat(m_io[m_ioMisoIndex].in, captureBuffer.upper(-1_b));

				*out = (BVec)captureBuffer;
				IF(lastEdge | bitPosCounterIn.value() == lastBitIndex)
				{
					valid(out) = '1';
					bitPosCounterIn.reset();
					captureBuffer = 0;
				}
			}
		}
	}

	IF(state.current() == CmdState::load_low & valid(command))
	{
		for (size_t i = 0; i < std::min<size_t>(8, m_io.size()); ++i)
			m_io[i].openDrain = (*command)[i];
		ready(command) = '1';
		state = CmdState::load_high;
	}

	IF(state.current() == CmdState::load_high & valid(command))
	{
		for (size_t i = 8; i < std::min<size_t>(16, m_io.size()); ++i)
			m_io[i].openDrain = (*command)[i - 8];
		ready(command) = '1';
		state = followupState.current();
	}

	for (Io& io : m_io)
	{
		io.out = reg(io.out, '0');
		io.en = reg(io.en, '0');
		io.openDrain = reg(io.openDrain, '0');
	}
	HCL_NAMED(m_io);
	for (Io& io : m_io)
		io.in = '0';

	HCL_NAMED(out);
	return out;
}

void gtry::scl::BitBangEngine::Io::pin(std::string prefix, PinNodeParameter param)
{
	in = tristatePin(iobufOut(), iobufEnable(), param).setName(prefix);
}

void gtry::scl::BitBangEngine::pin(std::string prefix, PinNodeParameter param)
{
	for (size_t i = 0; i < m_io.size(); ++i)
		m_io[i].pin(prefix + std::to_string(i), param);
}
