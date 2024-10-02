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

#include <gatery/frontend.h>

namespace gtry::scl
{
	UInt bitcount(Signal auto vec)
	{
		using namespace gtry::hlim;

		GroupScope entity(GroupScope::GroupType::ENTITY, "bitcount");
		entity
			.setComment("Counts the number of high bits");

		static_assert(std::is_same_v<Bit, std::remove_cvref_t<decltype(*begin(vec))>>, "bitcount only works on bit vectors");
		HCL_NAMED(vec);

		UInt sumOfOnes = ConstUInt(0, BitWidth::last(vec.size()));
		for (const auto& it : vec)
			sumOfOnes += (UInt)zext(it);
		HCL_NAMED(sumOfOnes);
		return sumOfOnes;
	}
}
