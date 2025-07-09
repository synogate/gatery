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
#include "EthernetPhy.h"
#include "../../stream/strm.h"

namespace gtry::scl::eth
{
	Reverse<TxRxStream> phy(RMII& in)
	{
		Area ent{ "scl_rmii_phy", true };

		TxRxStream out = { {2_b}, {2_b} };

		// tx side
		ready(out.tx) = '1';
		in.tx_en = reg(valid(out.tx), '0');
		in.txd = reg(*out.tx & valid(out.tx));

		// rx side
		**out.rx = *in.rxd;
		valid(*out.rx) = reg(valid(*out.rx), '0');
		IF(*in.crs_dv & (*in.rxd != 0))
			valid(*out.rx) = '1';
		IF(!(*in.crs_dv | reg(*in.crs_dv, '0'))) // RMII spec changed to toggle DV for the last byte to indicate last byte
			valid(*out.rx) = '0';

		return out;
	}

	RvStream<BVec> preambleFilter(RvStream<BVec> in)
	{
		Area ent{ "scl_preamble_filter", true };
		HCL_DESIGNCHECK_HINT(in->width() == 2_b, "not implemented");
		HCL_NAMED(in);

		enum class State
		{
			carrier, preamble, data
		};
		Reg<Enum<State>> state{ State::carrier };

		RvStream<BVec> out;
		out <<= in;

		IF(state.current() == State::carrier)
		{
			IF(valid(in) & *in == 1)
				state = State::preamble;
		}
		IF(state.current() == State::preamble)
		{
			IF(valid(in) & *in == 3)
				state = State::data;
		}
		IF(!valid(in))
			state = State::carrier;

		valid(out) &= state.current() == State::data;
		HCL_NAMED(out);
		return out;
	}

	RvStream<BVec> preambleInsert(RvStream<BVec> in)
	{
		Area ent{ "scl_preamble_filter", true };
		HCL_DESIGNCHECK_HINT(in->width() == 2_b, "not implemented");
		HCL_NAMED(in);

		enum class State
		{
			wait, preamble, data
		};
		Reg<Enum<State>> state{ State::wait };

		RvStream<BVec> out;
		out <<= in;

		IF(state.current() == State::wait)
		{
			valid(out) = '0';
			ready(in) = '0';
			IF(valid(in))
				state = State::preamble;
		}

		IF(state.current() == State::preamble)
		{
			ready(in) = '0';

			*out = 1;

			// 1 byte replaced by J/K in phy, 7 byte preamble, 1 byte SOF
			IF(scl::Counter(9*4).isLast())
			{
				*out = 3;
				state = State::data;
			}
		}

		IF(state.current() == State::data)
		{
			IF(!valid(in))
				state = State::wait;
		}

		HCL_NAMED(out);
		return out;
	}
}

