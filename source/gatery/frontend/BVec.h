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
	class BVec;
}

extern template class std::map<std::variant<gtry::BitVectorSliceStatic, gtry::BitVectorSliceDynamic>, gtry::BVec>;

namespace gtry {

/**
 * @addtogroup gtry_signals
 * @{
 */

	class BVecDefault : public BaseBitVectorDefault {
		public:
			explicit BVecDefault(const BVec& rhs);

			template<BitVectorIntegralLiteral Type>
			explicit BVecDefault(Type value);
			explicit BVecDefault(const char rhs[]);
	};

	class BVec : public SliceableBitVector<BVec, BVecDefault>
	{
	public:
		using Base = SliceableBitVector<BVec, BVecDefault>;
		using DefaultValue = BVecDefault;

		using Base::Base;
		BVec(BVec&& rhs) : Base(std::move(rhs)) { }
		BVec(const BVec &rhs) : Base(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.

		template<BitVectorIntegralLiteral Int>
		explicit BVec(Int vec) { assign((std::uint64_t) vec, Expansion::none); }
		explicit BVec(const char rhs[]) { assign(std::string_view(rhs), Expansion::none); }

		template<BitVectorIntegralLiteral Int>
		BVec& operator = (Int rhs) { assign((std::uint64_t) rhs, rhs < 0 ? Expansion::one : Expansion::zero); return *this; }
		BVec& operator = (const char rhs[]) { assign(std::string_view(rhs), Expansion::none); return *this; }

		// These must be here since they are implicitly deleted due to the cop and move ctors
		BVec& operator = (const BVec &rhs) { BaseBitVector::operator=(rhs); return *this; }
		BVec& operator = (BVec&& rhs) { BaseBitVector::operator=(std::move(rhs)); return *this; }

		virtual BVec toBVec() const override { return *this; }
		virtual void fromBVec(const BVec &bvec) override { (*this) = bvec; }
	};

	BVec ext(const BVec& bvec, BitWidth extendedWidth, Expansion policy);
	inline BVec zext(const BVec& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::zero); }
	inline BVec oext(const BVec& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::one); }
	inline BVec sext(const BVec& bvec, BitWidth extendedWidth) { return ext(bvec, extendedWidth, Expansion::sign); }

	BVec ext(const BVec& bvec, BitExtend increment, Expansion policy);
	inline BVec zext(const BVec& bvec, BitExtend increment = { 0 }) { return ext(bvec, increment, Expansion::zero); }
	inline BVec oext(const BVec& bvec, BitExtend increment = { 0 }) { return ext(bvec, increment, Expansion::one); }
	inline BVec sext(const BVec& bvec, BitExtend increment = { 0 }) { return ext(bvec, increment, Expansion::sign); }

	BVec ext(const BVec& bvec, BitReduce decrement, Expansion policy);
	inline BVec zext(const BVec& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::zero); }
	inline BVec oext(const BVec& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::one); }
	inline BVec sext(const BVec& bvec, BitReduce decrement) { return ext(bvec, decrement, Expansion::sign); }

	inline BVecDefault::BVecDefault(const BVec& rhs) : BaseBitVectorDefault(rhs) { }

	template<BitVectorIntegralLiteral Type>
	BVecDefault::BVecDefault(Type value) : BaseBitVectorDefault((std::uint64_t)value) { }
	inline BVecDefault::BVecDefault(const char rhs[]) : BaseBitVectorDefault(std::string_view(rhs)) { }

	inline BVec constructFrom(const BVec& value) { return BVec(value.width()); }

/**@}*/

}
