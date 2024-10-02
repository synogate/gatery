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
#include "Thermometric.h"
#include "BitCount.h"

namespace gtry::scl {

	BVec uintToThermometric(UInt in) {
		BVec ret = ConstBVec(BitWidth(in.width().last()));
		for (size_t i = 0; i < ret.size(); i++)
			ret[i] = in > i;
		return ret;
	}

	BVec uintToThermometric(UInt in, BitWidth outW) {
		return uintToThermometric(in).lower(outW);
	}

	BVec uintToThermometric(UInt in, const size_t inMaxValue) {
		return uintToThermometric(in).lower(BitWidth(inMaxValue));
	}

	UInt thermometricToUInt(BVec in) {
		return bitcount((UInt)in);
	}

	BVec emptyMaskGenerator(UInt in, BitWidth wordSize, BitWidth fullSize)
	{
		size_t maxInput = in.width().last();
		size_t numWords = fullSize.value / wordSize.value;
		BVec ret = ~ConstBVec(0, fullSize);
		for (size_t i = 0; i < numWords; i++) {
			size_t threshold = numWords-1 - i;
			Bit b;
			if (threshold <= maxInput)
				b = in <= threshold;
			else
				b = '0';
			ret.word(i, wordSize) = (BVec) sext(b);
		}
		return ret;
	}

}
