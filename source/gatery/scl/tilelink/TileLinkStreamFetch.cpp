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
#include "gatery/pch.h"
#include "TileLinkStreamFetch.h"
#include <gatery/scl/utils/OneHot.h>
#include <gatery/scl/utils/BitCount.h>

namespace gtry::scl
{
	TileLinkStreamFetch::TileLinkStreamFetch()
	{
		m_area.leave();
	}

	//static RvPacketStream<UInt> decompositionIntoPowersOfTwo(RvStream<UInt>&& input) {
	//
	//	UInt numberOfRequests = capture(bitcount(*input), valid(input));
	//	Counter requestCounter(numberOfRequests);
	//
	//	RvPacketStream<UInt> output = constructFrom(input).add(Eop);
	//
	//	IF(transfer(output)) {
	//		requestCounter.inc();
	//	}
	//	IF(transfer(input)) {
	//		requestCounter.reset();
	//	}
	//
	//	UInt leftOver = constructFrom(*input);
	//	leftOver = reg(leftOver, 0);
	//
	//	*output = biggestPowerOfTwo(leftOver);
	//
	//	IF(valid(input) & leftOver == 0)
	//		leftOver = *input;
	//	ELSE
	//		leftOver -= *output;
	//
	//	eop(output) = requestCounter.isLast();
	//	valid(output) = valid(in);
	//	ready(input) = ready(output) & requestCounter.isLast();
	//}


	TileLinkUB TileLinkStreamFetch::generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut, BitWidth sourceW)
	{
		auto ent = m_area.enter();
		HCL_NAMED(cmdIn);

		std::optional<BitWidth> size = std::nullopt;
		UInt logByteSize;
		if (m_maxBurstSizeInBits) {
			//calculate logByteSize from beats, assuming it fits well

			const size_t bitsPerBeat = dataOut->width().bits();

			UInt numBits = zext(cmdIn->beats, BitWidth::last(*m_maxBurstSizeInBits)) * bitsPerBeat;
			HCL_NAMED(numBits);
			IF(valid(cmdIn))	
				sim_assert(numBits % 8 == 0) << "not a full amount of bytes?";

			UInt numBytes = numBits.upper(-3_b);
			HCL_NAMED(numBytes);

			IF(valid(cmdIn))
				sim_assert(bitcount(numBytes) == 1) << "not a possible tileLink burst";
			logByteSize = encoder((OneHot) numBytes);
			HCL_NAMED(logByteSize);
			size = logByteSize.width();

		}


		auto link = tileLinkInit<TileLinkUB>(cmdIn->address.width(), dataOut->width(), sourceW, size);
		link.a->opcode = (size_t)TileLinkA::Get;
		link.a->param = 0;
		Bit isBurst = '0';
		if (m_maxBurstSizeInBits) {
			link.a->size = logByteSize;
			IF(logByteSize > utils::Log2C(link.a->mask.width().bits()))
				isBurst = '1';
		}
		else {
			link.a->size = utils::Log2C(link.a->mask.width().bits());
		}
		HCL_NAMED(isBurst);

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
			addressOffset += 1;
			IF(addressOffset == cmdIn->beats | isBurst) // this condition signals that the input command has been fulfilled. 
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

