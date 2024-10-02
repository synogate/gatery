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
		m_sum = b;
	else if (m_count == 1)
		m_carry = b;
	else
	{
		std::tie(m_sum, m_carry) = addCarrySave(m_sum, m_carry, b);
		m_carry <<= 1;
	}
	m_count++;
	return *this;
}

gtry::UInt gtry::scl::CarrySafeAdder::sum() const
{
	if (m_count <= 1)
		return m_sum;

	return m_sum + m_carry;
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

std::tuple<gtry::UInt, gtry::UInt> gtry::scl::addCarrySave(const UInt& a, const UInt& b, const UInt& c) 
{
	UInt sum = a ^ b ^ c;
	UInt carry = (a & c) | (a & b) | (c & b);
	return { sum, carry };
}
