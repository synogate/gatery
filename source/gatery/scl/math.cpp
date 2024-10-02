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
#include <gatery/scl_pch.h>
#include "math.h"

namespace gtry::scl{

	UInt biggestPowerOfTwo(const UInt& input) {
		UInt result = ConstUInt(0, input.width());
		for (size_t i = 0; i < input.width().bits(); i++){
			UInt candidate = 1 << i;
			IF(input.at(i) == '1')
				result = zext(candidate);
		}
		return result;
	}


	UInt longDivision(const UInt& numerator, const UInt& denominator, size_t stepsPerPipelineReg) {
		const BitWidth numW = numerator.width();
		const BitWidth denomW = denominator.width();

		UInt quotient = ConstUInt(numW);
		UInt remainder = cat(ConstUInt(0, denomW), numerator);
		for (size_t i = numW.bits(); i > 0; i--) {
			UInt& workingSlice = remainder(i - 1, denomW + 1_b);
			quotient[i-1] = workingSlice >= zext(denominator);
			IF(quotient[i-1])
				workingSlice -= zext(denominator);
			if (stepsPerPipelineReg != 0 && i % stepsPerPipelineReg == 0) 
				workingSlice = pipestage(workingSlice);
		}
		return quotient;
	}

	SInt longDivision(const SInt& numerator, const UInt& denominator, size_t stepsPerPipelineReg)
	{
		//To Sign Magnitude
		UInt numMagnitude = (UInt) numerator;
			IF(numerator.sign())
			numMagnitude = ~numMagnitude + 1;
		HCL_NAMED(numMagnitude);

		//Compute full result of division
		UInt resultMagnitude = longDivision(numMagnitude, denominator, stepsPerPipelineReg);
		HCL_NAMED(resultMagnitude);

		//Back to signed int.
		SInt resultSigned = (SInt) resultMagnitude;
		IF(numerator.sign())
			resultSigned = ~resultSigned + SInt(1);
		HCL_NAMED(resultSigned);

		return resultSigned;
	}

	



}
