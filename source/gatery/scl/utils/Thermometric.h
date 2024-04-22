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

/**
 * @brief Thermometric encoding, for the purpose of these helper functions, is defined with the following table
 * UInt	|	Thermometric (Unary variant)
 *	0	|	  0000000
 *	1	|	  0000001
 *	2	|	  0000011
 *	3	|	  0000111
 *	4	|	  0001111
 *	5	|	  0011111
 *	6	|	  0111111
 *	7	|	  1111111
*/
namespace gtry::scl {

	BVec uintToThermometric(UInt in);
	BVec uintToThermometric(UInt in, BitWidth outW);
	BVec uintToThermometric(UInt in, const size_t inMaxValue);

	UInt thermometricToUInt(BVec in);

	BVec emptyMaskGenerator(UInt in, BitWidth wordSize, BitWidth fullSize);

}
