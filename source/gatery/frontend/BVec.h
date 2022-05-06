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
 * @addtogroup gtry_signals Signals
 * @ingroup gtry_frontend
 * @brief All base signals
 * @{
 */


	class BVec;

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
		using Range = BVec::Range;
		using Base = SliceableBitVector<BVec, BVecDefault>;
		using DefaultValue = BVecDefault;

		BVec() = default;
		BVec(BVec&& rhs) : Base(std::move(rhs)) { }
		BVec(const BVecDefault &defaultValue) : Base(defaultValue) { }

		BVec(const SignalReadPort& port) : Base(port) { }
		BVec(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) : Base(node, range, expansionPolicy, initialScopeId) { } // alias
		BVec(BitWidth width, Expansion expansionPolicy = Expansion::none) : Base(width, expansionPolicy) { }

		BVec(const BVec &rhs) : Base(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.

		template<BitVectorIntegralLiteral Int>
		explicit BVec(Int vec) { assign((std::uint64_t) vec, Expansion::none); }
		explicit BVec(const char rhs[]) { assign(std::string_view(rhs), Expansion::none); }

		template<BitVectorIntegralLiteral Int>
		BVec& operator = (Int rhs) { assign((std::uint64_t) rhs, Expansion::none); return *this; }
		BVec& operator = (const char rhs[]) { assign(std::string_view(rhs), Expansion::none); return *this; }

		// These must be here since they are implicitly deleted due to the cop and move ctors
		BVec& operator = (const BVec &rhs) { BaseBitVector::operator=(rhs); return *this; }
		BVec& operator = (BVec&& rhs) { BaseBitVector::operator=(std::move(rhs)); return *this; }
	};

	BVec ext(const BVec& bvec, size_t increment, Expansion policy);
	inline BVec ext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::zero); }
	inline BVec zext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::zero); }
	inline BVec oext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::one); }
	inline BVec sext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::sign); }


	inline BVecDefault::BVecDefault(const BVec& rhs) : BaseBitVectorDefault(rhs) { }

	template<BitVectorIntegralLiteral Type>
	BVecDefault::BVecDefault(Type value) : BaseBitVectorDefault((std::uint64_t)value) { }
	inline BVecDefault::BVecDefault(const char rhs[]) : BaseBitVectorDefault(std::string_view(rhs)) { }

	inline BVec constructFrom(const BVec& value) { return BVec(value.width()); }

/**@}*/

}
