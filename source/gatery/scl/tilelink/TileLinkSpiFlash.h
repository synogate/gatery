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
#pragma once
#include <gatery/frontend.h>
#include <gatery/scl/io/QSpiController.h>
#include "tilelink.h"

namespace gtry::scl
{
	struct TileLinkSpiFlashConfig
	{
		BitWidth sourceW = 0_b;
		std::optional<BitWidth> sizeW;

		// command byte send to flash for quad io read
		uint8_t readQuadIOCode = 0xEB;

		// send command byte to flash ahead of time for improved random access latency
		// specifies how long the preloaded command should wait for a request before deselecting flash
		// a value of 0 is infinite
		std::optional<hlim::ClockRational> preloadCommandByteTimeout;

		// respond with tile link error to non get requests
		bool handleInvalidOpcodes = true;
	};

	TileLinkUB tileLinkSpiFlash(SpiPort& port, TileLinkSpiFlashConfig config = {})
	{
		// TODO: adapt to port width
		// TODO: support SDR phy by sending one command bit per cycle
		// TODO: optional support for deep sleep mode
		// TODO: support multiple chip select pins by adding more address bits
		// TODO: add ease of use wrapper with input a channel stall and d channel fifo

		Area ent{ "scl_tileLinkSpiFlash", true };
		TileLinkUB out = tileLinkInit<TileLinkUB>(24_b, port.out->width(), config.sourceW, config.sizeW);

		enum class State {
			idle,			// wait for request and chip deselect
			cmd,			// send command byte
			addr,			// send address to flash
			dummy,			// send m0 and wait during flash dummy cycles
			read,			// read as many cycles as burst data requested
			readWait,		// in case of spi latency, wait for burst data to be fully received

			// optional with preloadCommandByteTimeout
			preloadCmd,		// chip deselect and proceed to cmd state
			waitForRequest,	// in case of preloaded command byte wait for tl request

			// optional with handleInvalidOpcodes
			invalidRequest	// wait for burst request to be completed and respond with error
		};

		Reg<Enum<State>> state(State::idle);
		state.setName("state");

		std::optional<size_t> preloadCommandByteTimeoutCycles;
		if (config.preloadCommandByteTimeout)
			preloadCommandByteTimeoutCycles = hlim::ceil(ClockScope::getClk().absoluteFrequency() * *config.preloadCommandByteTimeout);

		size_t counterEnd = 4;
		// burst count
		counterEnd = std::max(counterEnd, 1ull << out.a->size.width().count());
		// timeout count
		if (preloadCommandByteTimeoutCycles)
			counterEnd = std::max(counterEnd, *preloadCommandByteTimeoutCycles);

		Counter counter(counterEnd);
		IF(state.next() != state.current())
			counter.reset();

		port.cs[0] = '1';
		port.clkEn = '1';
		port.inEn = '0';
		valid(port.out) = '0';
		*port.out = dontCare(*port.out);

		IF(state.current() == State::idle)
		{
			port.cs[0] = '0';
			port.clkEn = '0';
			IF(valid(out.a))
			{
				state = State::cmd;

				if(config.handleInvalidOpcodes)
					IF(!out.a->isGet())
						state = State::invalidRequest;
			}
		}

		if(config.preloadCommandByteTimeout)
			IF(state.current() == State::preloadCmd)
			{
				port.cs[0] = '0';
				state = State::cmd;
			}

		IF(state.current() == State::cmd)
		{
			valid(port.out) = '1';

			UInt cmdBits = "2b00";
			const UInt& cnt = counter.value().lower(2_b);
			for (size_t i = 0; i < 4; ++i)
				IF (cnt == i)
					cmdBits = config.readQuadIOCode >> 2 * i & 0x3;

			// command is read on lane 0 only, we pad the others with one to reduce transitions
			*port.out = (BVec)cat("3b111", cmdBits[1], "3b111", cmdBits[0]);
			IF(cnt == 3)
				state = config.preloadCommandByteTimeout ? State::waitForRequest : State::addr;
		}

		if(config.preloadCommandByteTimeout)
		{
			IF(state.combinatorial() == State::waitForRequest)
			{
				IF(valid(out.a))
				{
					state = State::addr;

					if (config.handleInvalidOpcodes)
						IF(!out.a->isGet())
							state = State::invalidRequest;
				}
			}

			IF(state.current() == State::waitForRequest)
			{
				port.clkEn = '0';

				if(*preloadCommandByteTimeoutCycles != 0)
					IF(counter.value() == *preloadCommandByteTimeoutCycles - 1)
						state = State::idle;
			}
		}

		IF(state.current() == State::addr)
		{
			valid(port.out) = '1';

			const UInt& cnt = counter.value().lower(2_b);
			IF(cnt == 0)
				*port.out = (BVec)out.a->address(16, 8_b);
			IF(cnt == 1)
				*port.out = (BVec)out.a->address(8, 8_b);
			IF(cnt == 2)
				*port.out = (BVec)out.a->address(0, 8_b);

			IF(cnt == 2)
				state = State::dummy;
		}

		IF(state.current() == State::dummy)
		{
			const UInt& cnt = counter.value().lower(2_b);

			// the first cycle is called "M" cycle where we need to send 0xFx
			valid(port.out) = cnt == 0;
			*port.out = 0xFF;

			IF(cnt[1])
				state = State::read;
		}

		IF(state.current() == State::read)
		{
			port.inEn = '1';
			UInt counterNext = counter.value() + 1 | name("counterNext");
			IF(counterNext[out.a->size])
				state = State::readWait;
		}

		IF(state.current() == State::readWait)
		{
			port.cs[0] = '0';
			port.clkEn = '0';
		}

		Bit endOfTransaction = (transfer(out.a) & eop(out.a)) | name("endOfTransaction");
		IF(endOfTransaction)
		{
			state = State::idle;
			if (config.preloadCommandByteTimeout)
				state = State::preloadCmd;
			if (config.handleInvalidOpcodes)
				IF(state.current() == State::invalidRequest)
					state = State::idle;
		}

		**out.d = tileLinkDefaultResponse(*out.a);
		valid(*out.d) = valid(*port.in);
		sim_assert(!valid(*out.d) | ready(*out.d)) << "backpressure not supported on incoming io data";

		(*out.d)->data = **port.in;

		// accept request
		ready(out.a) = transfer(*out.d) & eop(*out.d);

		if (config.handleInvalidOpcodes)
		{
			IF(state.current() == State::invalidRequest)
			{
				port.cs[0] = '0';
				port.clkEn = '0';

				(*out.d)->error = '1';
				ready(out.a) = '1';
				valid(*out.d) = endOfTransaction;
			}
		}

		HCL_NAMED(out);
		return out;
	}


}
