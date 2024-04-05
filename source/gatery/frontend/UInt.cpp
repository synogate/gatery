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
#include "UInt.h"
#include "BVec.h"

template class std::map<std::variant<gtry::BitVectorSliceStatic, gtry::BitVectorSliceDynamic>, gtry::UInt>;

namespace gtry 
{
	template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (const Selection&);
	template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (const Selection&) const;
	template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitWidth);
	template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitWidth) const;
	template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitReduce);
	template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitReduce) const;
	template UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitWidth);
	template const UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitWidth) const;
	template UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitReduce);
	template const UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitReduce) const;

	UInt::UInt(UInt&& rhs) : Base(std::move(rhs)) { }
	UInt::UInt(const UInt &rhs) : Base(rhs) { }
	UInt::UInt(const char rhs[]) { assign(std::string_view(rhs), Expansion::zero); }
	UInt& UInt::operator = (const char rhs[]) { assign(std::string_view(rhs), Expansion::zero); return *this; }
	UInt& UInt::operator = (const UInt &rhs) { BaseBitVector::operator=(rhs); return *this; }
	UInt& UInt::operator = (UInt&& rhs) { BaseBitVector::operator=(std::move(rhs)); return *this; }

	UInt::UInt() { }
	UInt::~UInt() { }

	UInt ext(const Bit& bit, BitWidth extendedWidth, Expansion policy)
	{
		HCL_DESIGNCHECK_HINT(extendedWidth.bits() != 0, "ext is not allowed to reduce width");

		SignalReadPort port = bit.readPort();
		port.expansionPolicy = policy;
		if (extendedWidth > 1_b)
			port = port.expand(extendedWidth.bits(), hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	UInt ext(const Bit& bit, BitExtend increment, Expansion policy)
	{
		SignalReadPort port = bit.readPort();
		port.expansionPolicy = policy;
		if (increment.value)
			port = port.expand(1 + increment.value, hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	UInt ext(const UInt& bvec, BitWidth extendedWidth, Expansion policy)
	{
		HCL_DESIGNCHECK_HINT(extendedWidth.bits() >= bvec.size(), "ext is not allowed to reduce width");

		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (extendedWidth > bvec.width())
			port = port.expand(extendedWidth.bits(), hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	UInt ext(const UInt& bvec, BitExtend increment, Expansion policy)
	{
		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (increment.value)
			port = port.expand(bvec.size() + increment.value, hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	UInt ext(const UInt& bvec, BitReduce decrement, Expansion policy)
	{
		HCL_DESIGNCHECK(decrement.value >= bvec.size());

		SignalReadPort port = bvec.readPort();
		port.expansionPolicy = policy;
		if (decrement.value)
			port = port.expand(bvec.size() - decrement.value, hlim::ConnectionType::BITVEC);
		return UInt(port);
	}

	BVec UInt::toBVec() const
	{
		return (BVec) *this;
	}

	void UInt::fromBVec(const BVec &bvec)
	{
		(*this) = (UInt) bvec;
	}

}
