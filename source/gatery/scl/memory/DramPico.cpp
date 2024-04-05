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
#include "DramPico.h"

namespace gtry::scl::sdram
{
	TileLinkUL dramPico(CommandBus& dram, const BVec& readData, size_t readLatency, BitWidth linkSourceW)
	{
		Area ent{ "scl_dramPico", true };
		HCL_DESIGNCHECK(dram.dq.width() == readData.width());

		TileLinkUL link;
		tileLinkInit(link, dram.a.width() + dram.ba.width() + 3, dram.dq.width(), BitWidth::count(dram.dqm.width().bits()), linkSourceW);
		
		dram.cke = reg(Bit{ '1' }, '0');
		dram.csn = !valid(link.a);

		size_t ofs = 0;
		dram.wen = link.a->address[ofs++];
		dram.rasn = link.a->address[ofs++];
		dram.casn = link.a->address[ofs++];
		dram.ba = (BVec)link.a->address(ofs, dram.ba.width()); 
		ofs += dram.ba.width().bits();
		dram.a = (BVec)link.a->address(ofs, dram.a.width()); 
		ofs += dram.a.width().bits();

		dram.dqm = link.a->mask;
		dram.dq = link.a->data;
		
		TileLinkChannelD response;
		valid(response) = valid(link.a);
		*response = tileLinkDefaultResponse(*link.a);

		valid(response).resetValue('0');
		for (size_t i = 0; i < readLatency; ++i)
			downstream(response) = reg(copy(downstream(response)));
		
		IF(response->opcode == (size_t)TileLinkD::AccessAckData)
			response->data = readData;

		*link.d <<= response;
		ready(link.a) = ready(*link.d);
		return link;
	}
}
