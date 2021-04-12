/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "Adder.h"

template class hcl::scl::Adder<hcl::BVec>;

hcl::scl::CarrySafeAdder& hcl::scl::CarrySafeAdder::add(const BVec& b)
{
	if (m_count == 0)
		m_sum = b;
	else if (m_count == 1)
		m_carry = b;
	else
	{
		BVec new_carry = (m_sum & m_carry) | (m_sum & b) | (m_carry & b);
		m_sum ^= m_carry ^ b;
		m_carry = new_carry << 1;
	}
	m_count++;
	return *this;
}

hcl::BVec hcl::scl::CarrySafeAdder::sum() const
{
	if (m_count <= 1)
		return m_sum;

	return m_sum + m_carry;
}
