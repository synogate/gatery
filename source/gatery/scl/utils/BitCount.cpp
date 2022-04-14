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
#include "BitCount.h"

#include <gatery/utils/BitManipulation.h>

namespace gtry::scl {

	UInt bitcount(UInt vec)
	{
		using namespace gtry;
		using namespace gtry::hlim;
		
		HCL_NAMED(vec);
		
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity
			.setName("bitcount")
			.setComment("Counts the number of high bits");
		
#if 1
		UInt sumOfOnes = BitWidth::last(vec.size());
		sumOfOnes = 0;
		for (auto i : utils::Range(vec.size()))
			sumOfOnes += zext(vec[i]);
		return sumOfOnes;
#else
		std::vector<UInt> subSums;
		subSums.resize(vec.width());
		for (auto i : utils::Range(vec.size()))
			subSums[i] = zext(vec[i], utils::Log2C(vec.size() + 1));

		for (uint64_t i = utils::nextPow2(vec.width().value)/2; i > 0; i /= 2) {
			for (uint64_t j = 0; j < i; j++) {
				if (j*2+0 >= subSums.size()) {
					subSums[j] = 0;
					continue;
				}
				if (j*2+1 >= subSums.size()) {
					subSums[j] = subSums[j*2+0];
					continue;
				}
				subSums[j] = subSums[j * 2 + 0] + subSums[j * 2 + 1];
			}
		}
		return subSums[0];
#endif
	}

 
}
