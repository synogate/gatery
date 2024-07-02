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
#include "../stream/Stream.h"
#include "../Counter.h"

namespace gtry::scl
{
	template<Signal ...MetaT>
	VStream<Bit, MetaT...> decodeNRZI(const VStream<Bit, MetaT...>& in, size_t stuffBitInterval)
	{
		auto scope = Area{ "scl_decodeNRZI" }.enter();
		auto out = in;

		// decode differential signals only
		IF(valid(in))
		{
			(*out) = (*in) == reg((*in), '0');

			if(stuffBitInterval)
			{
				Counter stuffCounter{ stuffBitInterval + 1 };
				stuffCounter.inc();
				IF(stuffCounter.isLast())
					valid(out) = '0';
				IF((*out) == '0')
					stuffCounter.reset();
			}
		}
		HCL_NAMED(out);
		return out;
	}

	VStream<UInt> encodeNRZI(const VStream<UInt>& in, size_t stuffBitInterval = 0);
}

