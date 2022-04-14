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
 * @{
 */

	class SInt;

	class SIntDefault : public BaseBitVectorDefault {
		public:
			explicit SIntDefault(const SInt& rhs);

			template<BitVectorIntegralLiteral Type>
			explicit SIntDefault(Type value);
			explicit SIntDefault(const char rhs[]);
	};

	class SInt : public SliceableBitVector<SInt, SIntDefault>
	{
	public:
		using Range = SInt::Range;
		using Base = SliceableBitVector<SInt, SIntDefault>;

		SInt() = default;
		SInt(SInt&& rhs) : Base(std::move(rhs)) { }
		SInt(const SIntDefault &defaultValue) : Base(defaultValue) { }

		SInt(const SignalReadPort& port) : Base(port) { }
		SInt(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) : Base(node, range, expansionPolicy, initialScopeId) { } // alias
		SInt(BitWidth width, Expansion expansionPolicy = Expansion::none) : Base(width, expansionPolicy) { }

		SInt(const SInt &rhs) : Base(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.


		template<BitVectorIntegralLiteral Int>
		explicit SInt(Int vec) { assign((std::int64_t) vec, Expansion::sign); }
		explicit SInt(const char rhs[]) { assign(std::string_view(rhs), Expansion::sign); }

		template<BitVectorIntegralLiteral Int>
		SInt& operator = (Int rhs) { assign((std::int64_t) rhs, Expansion::sign); return *this; }
		SInt& operator = (const char rhs[]) { assign(std::string_view(rhs), Expansion::sign); return *this; }

		// These must be here since they are implicitly deleted due to the cop and move ctors
		SInt& operator = (const SInt &rhs) { BaseBitVector::operator=(rhs); return *this; }
		SInt& operator = (SInt&& rhs) { BaseBitVector::operator=(std::move(rhs)); return *this; }

		Bit& sign() { return msb(); }
		const Bit& sign() const { return msb(); }
	};

	SInt ext(const SInt& bvec, size_t increment, Expansion policy);
	inline SInt ext(const SInt& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::sign); }
	inline SInt zext(const SInt& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::zero); }
	inline SInt oext(const SInt& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::one); }
	inline SInt sext(const SInt& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::sign); }


	inline SIntDefault::SIntDefault(const SInt& rhs) : BaseBitVectorDefault(rhs) { }

	template<BitVectorIntegralLiteral Type>
	SIntDefault::SIntDefault(Type value) : BaseBitVectorDefault((std::int64_t)value) { }
	inline SIntDefault::SIntDefault(const char rhs[]) : BaseBitVectorDefault(std::string_view(rhs)) { }

	inline SInt constructFrom(const SInt& value) { return SInt(value.width()); }

/**@}*/

}
