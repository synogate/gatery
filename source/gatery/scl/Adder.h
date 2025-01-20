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

namespace gtry::scl
{
	template<typename TVec = UInt>
	class Adder
	{
	public:
		Adder() = default;
		Adder(const Adder&) = default;

		template<typename TOperand>
		Adder& add(const TOperand& b)
		{
			if (m_count++ == 0)
				m_sum = b;
			else
				m_sum += b;
			return *this;
		}

		template<typename TOperand>
		Adder& operator + (const TOperand& b) { return add(b); }

		template<typename TOperand>
		Adder& operator += (const TOperand& b) { return add(b); }

		const TVec& sum() const { return m_sum; }
		operator const TVec& () const { return m_sum; }

	private:
		size_t m_count = 0;
		TVec m_sum;

	};

	extern template class gtry::scl::Adder<gtry::UInt>;

	struct CarrySaveValue
	{
		UInt sum;
		UInt carry;

		CarrySaveValue add(const UInt& summand) const;
		CarrySaveValue add(const UInt& summand, const Bit& subtract) const;

		CarrySaveValue operator + (const UInt& summand) const { return add(summand); }
	};

	class CarrySafeAdder
	{
	public:
		CarrySafeAdder() = default;
		CarrySafeAdder(const CarrySafeAdder&) = default;

		CarrySafeAdder& add(const UInt&);
		CarrySafeAdder operator + (const UInt& b) { CarrySafeAdder ret = *this; ret.add(b); return ret; }
		CarrySafeAdder& operator += (const UInt& b) { return add(b); }

		UInt sum() const;
		operator UInt () const { return sum(); }

		const UInt& intermediateSum() const { return m_val.sum; }
		const UInt& intermediateCarry() const { return m_val.carry; }

	private:
		size_t m_count = 0;
		CarrySaveValue m_val;
	};

	std::tuple<UInt, UInt> add(const UInt& a, const UInt& b, const Bit& cin = '0');
	CarrySaveValue addCarrySave(const UInt& a, const UInt& b, const UInt& c);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::CarrySaveValue, sum, carry);
