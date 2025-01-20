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
#include "gatery/scl_pch.h"
#include "Adder.h"

template class gtry::scl::Adder<gtry::UInt>;

gtry::scl::CarrySafeAdder& gtry::scl::CarrySafeAdder::add(const UInt& b)
{
	if (m_count == 0)
		m_val.sum = b;
	else if (m_count == 1)
		m_val.carry = b;
	else
	{
		m_val = addCarrySave(m_val.sum, m_val.carry, b);
		m_val.carry <<= 1;
	}
	m_count++;
	return *this;
}

gtry::UInt gtry::scl::CarrySafeAdder::sum() const
{
	if (m_count <= 1)
		return m_val.sum;

	return m_val.sum + m_val.carry;
}

std::tuple<gtry::UInt, gtry::UInt> gtry::scl::add(const UInt& a, const UInt& b, const Bit& cin)
{
	auto entity = Area{ "adder" }.enter();

	UInt sum = a + b + cin;
	HCL_NAMED(sum);

	UInt cout = ((a | b) & ~sum) | (a & b);
	HCL_NAMED(cout);

	return { sum, cout };
}

gtry::scl::CarrySaveValue gtry::scl::addCarrySave(const UInt& a, const UInt& b, const UInt& c)
{
	return {
		.sum = a ^ b ^ c,
		.carry = (a & c) | (a & b) | (c & b)
	};
}

gtry::scl::CarrySaveValue gtry::scl::CarrySaveValue::add(const UInt& summand) const
{
	gtry::scl::CarrySaveValue ret = addCarrySave(sum, carry, summand);
	ret.carry <<= 1;
	return ret;
}

gtry::scl::CarrySaveValue gtry::scl::CarrySaveValue::add(const UInt& summand, const Bit& subtract) const
{
	gtry::scl::CarrySaveValue ret = addCarrySave(
		sum, 
		cat(carry.upper(-1_b), subtract), 
		summand ^ subtract
	);
	ret.carry <<= 1;
	return ret;
}
