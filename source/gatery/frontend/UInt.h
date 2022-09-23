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

/**
 * @addtogroup gtry_signals
 * @{
 */

	class UInt;

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
		using Range = BaseBitVector::Range;
		using Base = SliceableBitVector<UInt, UIntDefault>;
		using DefaultValue = UIntDefault;

		UInt() = default;
		UInt(UInt&& rhs) : Base(std::move(rhs)) { }
		UInt(const UIntDefault &defaultValue) : Base(defaultValue) { }

		UInt(const SignalReadPort& port) : Base(port) { }
		UInt(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) : Base(node, range, expansionPolicy, initialScopeId) { } // alias
		UInt(BitWidth width, Expansion expansionPolicy = Expansion::none) : Base(width, expansionPolicy) { }

		UInt(const UInt &rhs) : Base(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.

		template<BitVectorIntegralLiteral Int>
		UInt(Int vec) { HCL_DESIGNCHECK_HINT(vec >= 0, "Can not assign negative values to UInt"); assign((std::uint64_t)vec, Expansion::zero); }
		UInt(const char rhs[]) { assign(std::string_view(rhs), Expansion::zero); }

		template<BitVectorIntegralLiteral Int>
		UInt& operator = (Int rhs) { HCL_DESIGNCHECK_HINT(rhs >= 0, "Can not assign negative values to UInt"); assign((std::uint64_t)rhs, Expansion::zero); return *this; }
		UInt& operator = (const char rhs[]) { assign(std::string_view(rhs), Expansion::zero); return *this; }

		// These must be here since they are implicitly deleted due to the cop and move ctors
		UInt& operator = (const UInt &rhs) { BaseBitVector::operator=(rhs); return *this; }
		UInt& operator = (UInt&& rhs) { BaseBitVector::operator=(std::move(rhs)); return *this; }
	};

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
