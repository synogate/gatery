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
#include <gatery/pch.h>
#include "math.h"

namespace gtry::scl{

	template <Signal T>
	T min(const T& a, const T& b){
		BitWidth retW = std::min(a.width(), b.width());
		T ret = retW;
		ret = a;
		IF(a > b) ret = b;
		return ret;
	}
	template <Signal T>
	T max(const T& a, const T& b) {
		BitWidth retW = std::max(a.width(), b.width());
		T ret = retW;
		ret = a;
		IF(a < b) ret = b;
		return ret;
	}

	UInt biggestPowerOfTwo(UInt input) {
		UInt result = ConstUInt(0, input.width());
		for (size_t i = 0; i < input.width().bits(); i++){
			UInt candidate = 1 << i;
			IF(input.at(i) == '1')
				result = zext(candidate);
		}
		return result;
	}
}
