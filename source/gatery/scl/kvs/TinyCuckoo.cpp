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
#include "TinyCuckoo.h"
#include "../crypto/SipHash.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::scl
{
	template class TinyCuckoo<BVec, BVec>;

	TinyCuckooOut tinyCuckoo(const TinyCuckooIn& in)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TinyCuckoo");

		TinyCuckooOut out;
		out.found = '0';
		out.hash = in.hash;
		out.key = in.key;
		out.userData = in.userData;
		out.value = zext(0, in.valueWidth().value);

		for(size_t l = 0; l < in.latency; l++)
			out = reg(out);

		for (size_t i = 0; i < in.numTables; ++i)
		{
			GroupScope entity(GroupScope::GroupType::ENTITY);
			entity.setName("TinyCuckooTable");

			Memory<TinyCuckooItem> mem(1ull << in.tableWidth().value, in.update.item);
			//mem.setType(MemType::MEDIUM);
			mem.initZero();

			IF(in.update.valid & in.update.tableIdx == i)
				mem[in.update.itemIdx] = in.update.item;

			SymbolSelect hashPart{ in.tableWidth().value };
			BVec lookupAddress = in.hash(hashPart[i]);
			HCL_NAMED(lookupAddress);

			TinyCuckooItem lookupData = mem[lookupAddress];
			for (size_t l = 0; l < in.latency; l++)
				lookupData = reg(lookupData);
			HCL_NAMED(lookupData);

			IF(lookupData.valid & (lookupData.key == out.key))
			{
				out.found = '1';
				out.value = lookupData.value;
			}
		}

		HCL_NAMED(out);
		return out;
	}

}
