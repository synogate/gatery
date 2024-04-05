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
#include "TileLinkStreamFetch.h"
#include <gatery/scl/utils/OneHot.h>
#include <gatery/scl/utils/BitCount.h>

namespace gtry::scl
{
	TileLinkStreamFetch::TileLinkStreamFetch()
	{
		m_area.leave();
	}

	TileLinkUB TileLinkStreamFetch::generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut, BitWidth sourceW)
	{
		auto ent = m_area.enter();
		HCL_NAMED(cmdIn);

		std::optional<BitWidth> size;
		UInt logByteSize;

		if (m_maxBurstSizeInBits) {

			const UInt bitsPerBeat = dataOut->width().bits();

			UInt numBitsTotal = zext(cmdIn->beats) * zext(bitsPerBeat, cmdIn->beats.width() + bitsPerBeat.width());
			HCL_NAMED(numBitsTotal);

			IF(valid(cmdIn)) {
				sim_assert(numBitsTotal % 8 == 0) << "not a full amount of bytes";
				sim_assert(numBitsTotal % *m_maxBurstSizeInBits == 0) << "the specified amount of beats is not a full amount of bursts.";
			}

			UInt numBytesTotal = numBitsTotal.upper(-3_b);
			HCL_NAMED(numBytesTotal);

			HCL_DESIGNCHECK_HINT(*m_maxBurstSizeInBits % 8 == 0, "max burst size must be multiple bytes");
			UInt numBytesBurst = *m_maxBurstSizeInBits / 8;

			IF(valid(cmdIn))
				sim_assert(bitcount(numBytesBurst) == 1) << "TileLink Bursts have to be a power of two amount of bytes";

			logByteSize = encoder((OneHot) numBytesBurst);
			HCL_NAMED(logByteSize); 
			size = logByteSize.width();
		}


		auto link = tileLinkInit<TileLinkUB>(cmdIn->address.width(), dataOut->width(), sourceW, size);
		link.a->opcode = (size_t)TileLinkA::Get;
		link.a->param = 0;
		if (m_maxBurstSizeInBits) {
			link.a->size = logByteSize;
		}
		else {
			link.a->size = utils::Log2C(link.a->mask.width().bits());
		}

		//link.a->source = 0;
		link.a->mask = link.a->mask.width().mask(); //only full bus reads
		link.a->data = ConstBVec(link.a->data.width());

		//here we calculate the address for the next request.
		UInt addressOffset = cmdIn->beats.width();
		HCL_NAMED(addressOffset);
		addressOffset = reg(addressOffset, 0);
		link.a->address = cmdIn->address + zext(cat(addressOffset, ConstUInt(0, BitWidth::count(dataOut->width().bytes()))));

		BVec readySource{ BitWidth{sourceW.count()} };
		HCL_NAMED(readySource);

		VStream<UInt> nextSource = priorityEncoder((UInt)readySource);
		HCL_NAMED(nextSource);
		
		link.a->source = *nextSource;
		valid(link.a) = valid(cmdIn) & valid(nextSource);

		if (m_pauseFetch)
		{
			IF(*m_pauseFetch)
				valid(link.a) = '0';
			setName(*m_pauseFetch, "pauseFetch");
		}

		IF(transfer(*link.d) & eop(*link.d)) {
			if ((*link.d)->source.width() == 0_b)
				readySource[0] = '1';
			else 
				readySource[(*link.d)->source] = '1';
		}
		IF(transfer(link.a)) {
			if (link.a->source.width() == 0_b)
				readySource[0] = '0';
			else 
				readySource[link.a->source] = '0';
		}


		readySource = reg(readySource, BVec{ readySource.width().mask() });

		ready(cmdIn) = '0';
		IF(transfer(link.a))
		{
			if (m_maxBurstSizeInBits)
				addressOffset += (*m_maxBurstSizeInBits / dataOut->width().bits());
			else
				addressOffset += 1;
			IF(addressOffset == cmdIn->beats)
			{
				ready(cmdIn) = '1';
				addressOffset = 0;
			}
		}

		*dataOut = (*link.d)->data;
		valid(dataOut) = valid(*link.d);
		ready(*link.d) = ready(dataOut);
		HCL_NAMED(dataOut);
		
		HCL_NAMED(link);
		return link;
	}
}

namespace gtry 
{
	template void connect(scl::TileLinkStreamFetch::Command&, scl::TileLinkStreamFetch::Command&);
#if !defined(__clang__) || __clang_major__ >= 14
	template auto upstream(scl::TileLinkStreamFetch::Command&);
	template auto downstream(scl::TileLinkStreamFetch::Command&);
#endif
}