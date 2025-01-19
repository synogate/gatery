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
	};


	TileLinkUB tileLinkSpiFlash(SpiPort& port, TileLinkSpiFlashConfig config = {})
	{
		// TODO: adapt to port width
		// TODO: send error response on invalid request
		// TODO: use counter instead of redundand states
		// TODO: presend command code to reduce latency, but with timeout
		// TODO: optional support for deep sleep mode
		// TODO: support multiple chip select pins by adding more address bits
		// TODO: add ease of use wrapper with input a channel stall and d channel fifo

		Area ent{ "scl_tileLinkSpiFlash", true };
		TileLinkUB out = tileLinkInit<TileLinkUB>(24_b, port.out->width(), config.sourceW, config.sizeW);

		enum class State {
			idle, 
			cmd0, cmd1, cmd2, cmd3,
			addr0, addr1, addr2, m0,
			dummy0, dummy1,
			read,
			readWait
		};

		Reg<Enum<State>> state(State::idle);
		state.setName("state");

		port.cs[0] = state.current() != State::idle;
		port.clkEn = state.current() != State::idle;
		port.inEn = '0';
		valid(port.out) = '0';
		*port.out = dontCare(*port.out);

		IF(state.current() == State::idle)
		{
			IF(valid(out.a))
				state = State::cmd0;
		}
		// quad read i/o command 0xEB = 0b11'10'10'11
		IF(state.current() == State::cmd0)
		{
			valid(port.out) = '1';
			*port.out = 0xFF;
			state = State::cmd1;
		}

		IF(state.current() == State::cmd1)
		{
			valid(port.out) = '1';
			*port.out = 0xFE;
			state = State::cmd2;
		}

		IF(state.current() == State::cmd2)
		{
			valid(port.out) = '1';
			*port.out = 0xFE;
			state = State::cmd3;
		}

		IF(state.current() == State::cmd3)
		{
			valid(port.out) = '1';
			*port.out = 0xFF;
			state = State::addr0;
		}

		IF(state.current() == State::addr0)
		{
			valid(port.out) = '1';
			*port.out = (BVec)out.a->address(16, 8_b);
			state = State::addr1;
		}

		IF(state.current() == State::addr1)
		{
			valid(port.out) = '1';
			*port.out = (BVec)out.a->address(8, 8_b);
			state = State::addr2;
		}

		IF(state.current() == State::addr2)
		{
			valid(port.out) = '1';
			*port.out = (BVec)out.a->address(0, 8_b);
			state = State::m0;
		}

		IF(state.current() == State::m0)
		{
			valid(port.out) = '1';
			*port.out = 0xFF;
			state = State::dummy0;
		}

		IF(state.current() == State::dummy0)
			state = State::dummy1;

		IF(state.current() == State::dummy1)
			state = State::read;

		IF(state.current() == State::read)
		{
			port.inEn = '1';

			UInt len = transferLengthFromLogSize(out.a->size, 1);
			IF(Counter(len).isLast())
				state = State::readWait;
		}

		IF(transfer(out.a))
			state = State::idle;

		**out.d = tileLinkDefaultResponse(*out.a);
		valid(*out.d) = valid(*port.in);
		sim_assert(!valid(*out.d) | ready(*out.d)) << "backpressure not supported on incoming io data";

		(*out.d)->data = **port.in;

		// accept request
		ready(out.a) = transfer(*out.d) & eop(*out.d);

		HCL_NAMED(out);
		return out;
	}


}
