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
#include "cdc.h"

gtry::BVec gtry::scl::grayEncode(UInt val)
{
	return (BVec)(val ^ (val >> 1));
}

gtry::UInt gtry::scl::grayDecode(BVec val)
{
	UInt ret = ConstUInt(0, val.width());

	ret.msb() = val.msb();
	for(size_t i = ret.width().bits() - 2; i < ret.width().bits(); i--)
		ret[i] = ret[i + 1] ^ val[i];

	return ret;
}

gtry::UInt gtry::scl::grayCodeSynchronize(UInt in, const Clock& inClock, const Clock& outClock, size_t outStages, bool inStage)
{
	return grayDecode(synchronize(grayEncode(in), inClock, outClock, outStages, inStage));
}