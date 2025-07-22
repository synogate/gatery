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

	TileLinkU TileLinkStreamFetch::generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut, BitWidth sourceW)
	{
		auto ent = m_area.enter();
		HCL_NAMED(cmdIn);

		HCL_DESIGNCHECK_HINT(!m_maxBurstSizeInBits || *m_maxBurstSizeInBits % 8 == 0, "max burst size must be multiple bytes");
		HCL_DESIGNCHECK_HINT(!m_maxBurstSizeInBits || utils::isPow2(*m_maxBurstSizeInBits), "TileLink Bursts have to be a power of two amount of bytes");

		// calculate burt size
		std::optional<UInt> size;
		if(m_maxBurstSizeInBits)
			size = utils::Log2(*m_maxBurstSizeInBits / 8);

		std::optional<BitWidth> sizeW;
		if (size)
			sizeW = size->width();

		// calculate address
		UInt addressOffset = cmdIn->beats.width();
		HCL_NAMED(addressOffset);
		addressOffset = reg(addressOffset, 0);

		// calculate source
		BVec readySource{ BitWidth{sourceW.count()} };
		VStream<UInt> nextSource = priorityEncoder((UInt)readySource);
		HCL_NAMED(nextSource);

		// request
		auto link = tileLinkInit<TileLinkU>(cmdIn->address.width(), dataOut->width(), sourceW, sizeW);
		link.a->setupGet(
			cmdIn->address + zext(cat(addressOffset, ConstUInt(0, BitWidth::count(dataOut->width().bytes())))),
			*nextSource,
			size
		);
		
		valid(link.a) = valid(cmdIn) & valid(nextSource);

		if (m_pauseFetch)
		{
			IF(*m_pauseFetch)
				valid(link.a) = '0';
			setName(*m_pauseFetch, "pauseFetch");
		}

		IF(transfer(*link.d) & eop(*link.d))
			readySource[(*link.d)->source] = '1';
		IF(transfer(link.a))
			readySource[link.a->source] = '0';
		readySource = reg(readySource, BVec{ readySource.width().mask() }) | name("readySource");

		ready(cmdIn) = '0';
		IF(transfer(link.a))
		{
			UInt nextAddressOffset =
				zext(addressOffset, +1_b) +
				m_maxBurstSizeInBits.value_or(dataOut->width().bits()) / dataOut->width().bits()
				| name("nextAddressOffset");

			addressOffset = nextAddressOffset.lower(-1_b);
			IF(nextAddressOffset >= zext(cmdIn->beats))
			{
				ready(cmdIn) = '1';
				addressOffset = 0;
			}
		}

		dataOut <<= transform(move(*link.d), &TileLinkD::data);
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
