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

namespace gtry::scl
{
	TileLinkStreamFetch::TileLinkStreamFetch()
	{
		m_area.leave();
	}

	TileLinkUL TileLinkStreamFetch::generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut)
	{
		auto ent = m_area.enter();
		HCL_NAMED(cmdIn);

		TileLinkUL link;
		tileLinkInit(link, 
			cmdIn->address.width(), 
			dataOut->width(), 
			BitWidth::last(utils::Log2C(cmdIn->address.width().bytes())), 
			0_b
		);
		
		link.a->opcode = (size_t)TileLinkA::Get;
		link.a->param = 0;
		link.a->size = utils::Log2C(link.a->mask.width().bits());
		link.a->source = 0;
		link.a->mask = link.a->mask.width().mask();
		link.a->data = ConstBVec(link.a->data.width());

		UInt addressOffset = cmdIn->beats.width();
		HCL_NAMED(addressOffset);
		addressOffset = reg(addressOffset, 0);
		link.a->address = cmdIn->address + zext(cat(addressOffset, ConstUInt(0, BitWidth::count(dataOut->width().bytes()))));

		Bit busy = flag(transfer(link.a), transfer(*link.d)) & !transfer(*link.d);
		HCL_NAMED(busy);
		valid(link.a) = valid(cmdIn) & !busy;
		ready(cmdIn) = '0';
		IF(transfer(link.a))
		{
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

