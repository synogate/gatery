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
#include "BVec.h"

template class std::map<std::variant<gtry::BitVectorSliceStatic, gtry::BitVectorSliceDynamic>, gtry::BVec>;

namespace gtry 
{
	BVec ext(const BVec& bvec, BitWidth extendedWidth, Expansion policy)
	{
		HCL_DESIGNCHECK_HINT(extendedWidth.bits() >= bvec.size(), "ext is not allowed to reduce width");

		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (extendedWidth > bvec.width())
			port = port.expand(extendedWidth.bits(), hlim::ConnectionType::BITVEC);
		return BVec(port);
	}

	BVec ext(const BVec& bvec, BitExtend increment, Expansion policy)
	{
		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (increment.value)
			port = port.expand(bvec.size() + increment.value, hlim::ConnectionType::BITVEC);
		return BVec(port);
	}

	BVec ext(const BVec& bvec, BitReduce decrement, Expansion policy)
	{
		HCL_DESIGNCHECK(decrement.value >= bvec.size());

		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (decrement.value)
			port = port.expand(bvec.size() - decrement.value, hlim::ConnectionType::BITVEC);
		return BVec(port);
	}
}
