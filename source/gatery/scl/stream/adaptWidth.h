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
#include "Stream.h"
#include "../Counter.h"

namespace gtry::scl
{
	//Stream<UInt> adaptWidth(Stream<UInt>& source, BitWidth width, Bit reset = '0');
	//Stream<Packet<UInt>> adaptWidth(Stream<Packet<UInt>>& source, BitWidth width);

	template<Signal Targ, class Tproc>
	auto transform(Stream<Targ>& source, Tproc&& func);

	template<StreamSignal T> T eraseBeat(T&& source, UInt beatOffset, UInt beatCount);
	template<StreamSignal T> T eraseLastBeat(T&& source);
	template<StreamSignal T, SignalValue Tval> T insertBeat(T&& source, UInt beatOffset, const Tval& value);
}


namespace gtry::scl
{
	template<Signal Targ, class Tproc>
	auto transform(Stream<Targ>& source, Tproc&& func)
	{
		auto scope = Area{ "scl_transform_stream" }.enter();
		auto&& newVal = std::invoke(func, source.data);

		Stream<std::remove_reference_t<decltype(newVal)>> ret{
			.valid = source.valid,
			.data = std::forward<decltype(newVal)>(newVal)
		};
		*source.ready = *ret.ready;
		return ret;
	}

	template<StreamSignal T> 
	T eraseBeat(T&& source, UInt beatOffset, UInt beatCount)
	{
		BitWidth beatLimit = std::max(beatOffset.width(), beatCount.width()) + 1;
		UInt beatCounter = beatLimit;

		T ret;
		ret <<= source;

		IF(beatCounter >= zext(beatOffset) & beatCounter < zext(beatOffset + beatCount))
		{
			valid(ret) = '0';
			ready(source) = '1';
		}

		IF(transfer(source))
		{
			IF(beatCounter < zext(beatOffset + beatCount))
				beatCounter += 1;
			IF(eop(source))
				beatCounter = 0;
		}
		beatCounter = reg(beatCounter, 0);
		return ret;
	}

	template<StreamSignal T> 
	T eraseLastBeat(T&& source)
	{
		T in;
		in <<= source;

		T ret = constructFrom(in);
		IF(eop(source))
			in.valid = '0';
		ret = in.regDownstream();

		Bit eopReg = flag(eop(source) & valid(source), transfer(ret));
		IF(eop(source) | eopReg)
			eop(ret) = '1';
		return ret;
	}

	template<StreamSignal T, SignalValue Tval> 
	T insertBeat(T&& source, UInt beatOffset, const Tval& value)
	{
		UInt beatCounter = beatOffset.width() + 1;

		T ret;
		ret <<= source;

		IF(beatCounter == zext(beatOffset))
		{
			*ret = value;
			ready(source) = '0';
		}

		IF(transfer(ret))
		{
			IF(beatCounter < zext(beatOffset + 1))
				beatCounter += 1;
			IF(eop(source))
				beatCounter = 0;
		}
		beatCounter = reg(beatCounter, 0);
		return ret;
	}
}
