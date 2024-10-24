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
\*/
#include "gatery/pch.h"
#include "BitManipulation.h"

namespace gtry::utils 
{
	UndefinedValueIterator::iterator::iterator(std::uint64_t value, std::uint64_t defined, std::uint64_t maximum)
	{
		isEndIterator = false;
		this->defined = defined;
		this->maximum = maximum;
		this->value = value & defined;
	}
	
	UndefinedValueIterator::iterator::iterator()
	{
		isEndIterator = true;
	}

	void UndefinedValueIterator::iterator::operator++() {

		if (value == maximum) {
			isEndIterator = true;
			return;
		}
		auto definedValue = value & defined;
		auto undefinedValue = value & ~defined;
		undefinedValue++;

		std::uint64_t overflow;
		while ((overflow = undefinedValue & defined) != 0) {
			auto newUndefinedValue = undefinedValue + overflow;
			if (newUndefinedValue <= undefinedValue || (definedValue | newUndefinedValue) > maximum) {
				isEndIterator = true;
				return;
			}
			undefinedValue = newUndefinedValue;
		}
		value = definedValue | undefinedValue;
	}
}