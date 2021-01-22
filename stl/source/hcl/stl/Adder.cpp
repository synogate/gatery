#include "Adder.h"

template class hcl::stl::Adder<hcl::BVec>;

hcl::stl::CarrySafeAdder& hcl::stl::CarrySafeAdder::add(const BVec& b)
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

hcl::BVec hcl::stl::CarrySafeAdder::sum() const
{
	if (m_count <= 1)
		return m_sum;

	return m_sum + m_carry;
}
