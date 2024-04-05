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

#include "BitVector.h"

namespace gtry {
	class UInt;
}

extern template class std::map<std::variant<gtry::BitVectorSliceStatic, gtry::BitVectorSliceDynamic>, gtry::UInt>;

namespace gtry {

/**
 * @addtogroup gtry_signals
 * @{
 */

	class UIntDefault : public BaseBitVectorDefault {
		public:
			explicit UIntDefault(const UInt& rhs);

			template<BitVectorIntegralLiteral Type>
			explicit UIntDefault(Type value);
			explicit UIntDefault(const char rhs[]);
	};

	class UInt : public SliceableBitVector<UInt, UIntDefault>
	{
	public:
		using Base = SliceableBitVector<UInt, UIntDefault>;
		using DefaultValue = UIntDefault;

		using Base::Base;
		UInt(UInt&& rhs);
		UInt(const UInt &rhs);

		template<BitVectorIntegralLiteral Int>
		UInt(Int vec) : UInt() { HCL_DESIGNCHECK_HINT(vec >= 0, "Can not assign negative values to UInt"); assign((std::uint64_t)vec, Expansion::zero); }
		UInt(const char rhs[]);

		// make explicit in cpp to force instantiation of base class template there (and only there)
		UInt();
		~UInt();

		template<BitVectorIntegralLiteral Int>
		UInt& operator = (Int rhs) { HCL_DESIGNCHECK_HINT(rhs >= 0, "Can not assign negative values to UInt"); assign((std::uint64_t)rhs, Expansion::zero); return *this; }
		UInt& operator = (const char rhs[]);

		// These must be here since they are implicitly deleted due to the cop and move ctors
		UInt& operator = (const UInt &rhs);
		UInt& operator = (UInt&& rhs);

		virtual BVec toBVec() const override;
		virtual void fromBVec(const BVec &bvec) override;
	};

	// By far the most common operations!
	extern template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (const Selection&);
	extern template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (const Selection&) const;
	extern template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitWidth);
	extern template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitWidth) const;
	extern template UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitReduce);
	extern template const UInt& SliceableBitVector<UInt, UIntDefault>::operator() (size_t, BitReduce) const;

	extern template UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitWidth);
	extern template const UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitWidth) const;
	extern template UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitReduce);
	extern template const UInt& SliceableBitVector<UInt, UIntDefault>::lower(BitReduce) const;



	UInt ext(const Bit& bit, BitWidth extendedWidth, Expansion policy);
	inline UInt ext(const Bit& bit, BitWidth extendedWidth) { return ext(bit, extendedWidth, Expansion::zero); }
	inline UInt zext(const Bit& bit, BitWidth extendedWidth) { return ext(bit, extendedWidth, Expansion::zero); }
	inline UInt oext(const Bit& bit, BitWidth extendedWidth) { return ext(bit, extendedWidth, Expansion::one); }
	inline UInt sext(const Bit& bit, BitWidth extendedWidth) { return ext(bit, extendedWidth, Expansion::sign); }

	UInt ext(const Bit& bit, BitExtend increment, Expansion policy);
	inline UInt ext(const Bit& bit, BitExtend increment = { 0 }) { return ext(bit, increment, Expansion::zero); }
	inline UInt zext(const Bit& bit, BitExtend increment = { 0 }) { return ext(bit, increment, Expansion::zero); }
	inline UInt oext(const Bit& bit, BitExtend increment = { 0 }) { return ext(bit, increment, Expansion::one); }
	inline UInt sext(const Bit& bit, BitExtend increment = { 0 }) { return ext(bit, increment, Expansion::sign); }

	UInt ext(const UInt& bvec, BitWidth extendedWidth, Expansion policy);
	inline UInt ext(const UInt& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::zero); }
	inline UInt zext(const UInt& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::zero); }
	inline UInt oext(const UInt& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::one); }
	inline UInt sext(const UInt& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::sign); }

	UInt ext(const UInt& bvec, BitExtend increment, Expansion policy);
	inline UInt ext(const UInt& bvec, BitExtend increment = {0}) { return ext(bvec, increment, Expansion::zero); }
	inline UInt zext(const UInt& bvec, BitExtend increment = {0}) { return ext(bvec, increment, Expansion::zero); }
	inline UInt oext(const UInt& bvec, BitExtend increment = {0}) { return ext(bvec, increment, Expansion::one); }
	inline UInt sext(const UInt& bvec, BitExtend increment = {0}) { return ext(bvec, increment, Expansion::sign); }

	UInt ext(const UInt& bvec, BitReduce decrement, Expansion policy);
	inline UInt ext(const UInt& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::zero); }
	inline UInt zext(const UInt& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::zero); }
	inline UInt oext(const UInt& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::one); }
	inline UInt sext(const UInt& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::sign); }

	inline UIntDefault::UIntDefault(const UInt& rhs) : BaseBitVectorDefault(rhs) { }

	template<BitVectorIntegralLiteral Type>
	UIntDefault::UIntDefault(Type value) : BaseBitVectorDefault((std::uint64_t)value) { }
	inline UIntDefault::UIntDefault(const char rhs[]) : BaseBitVectorDefault(std::string_view(rhs)) { }

	inline UInt constructFrom(const UInt& value) { return UInt(value.width()); }

/**@}*/

}
