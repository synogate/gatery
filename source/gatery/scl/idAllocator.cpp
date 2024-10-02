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
#include <gatery/scl_pch.h>

#include "idAllocator.h"

#include "Fifo.h"
#include "Counter.h"
#include "stream/StreamArbiter.h"
#include "stream/streamFifo.h"
#include "stream/utils.h"

namespace gtry::scl
{
	RvStream<UInt> idAllocator(VStream<UInt> free, std::optional<size_t> numIds)
	{
		Area ent{ "scl_idAllocator", true };
		HCL_NAMED(free);

		size_t idLimit = numIds.value_or(free->width().count());
		HCL_DESIGNCHECK(idLimit <= free->width().count());

		auto in = (RvStream<UInt>)free; // add and ignore ready. fifo cannot overflow.
		HCL_NAMED(in);
		sim_assert(ready(in) | !valid(in)) << "freed more id's than we have";
		auto outFiFo = strm::fifo(move(in), idLimit);
		HCL_NAMED(outFiFo);

		RvStream<UInt> outCounter;
		Counter idCounter{ idLimit + 1 };
		valid(outCounter) = !idCounter.isLast();
		IF(transfer(outCounter))
			idCounter.inc();
		*outCounter = resizeTo(zext(idCounter.value()), free->width());
		HCL_NAMED(outCounter);

		RvStream<UInt> out = strm::arbitrate(move(outFiFo), move(outCounter));
		HCL_NAMED(out);
		return out;
	}

	RvStream<UInt> idAllocatorInOrder(Bit free, size_t numIds)
	{
		Area area{ "scl_idAllocatorInOrder", true };

		RvStream<UInt> out{ BitWidth::count(numIds) };

		Counter idAllocator(numIds);
		IF(transfer(out))
			idAllocator.inc();
		*out = idAllocator.value();
		valid(out) = '1';

		return move(out) | strm::allowanceStall(free, BitWidth::last(numIds), numIds);
	}
}
