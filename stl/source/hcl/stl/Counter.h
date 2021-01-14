#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	class Counter
	{
	public:
		Counter(size_t end) :
			m_value{BitWidth{utils::Log2C(end)}}
		{
			m_last = m_value == end - 1;
			m_value = reg(m_value + 1, 0);
			IF(m_last)
				m_value = 0;
		}
		
		void reset() { m_value = 0; }
		const BVec& value() const { return m_value; }
		const Bit& isLast() const { return m_last; }
		Bit isFirst() const { return m_value == 0; }

	private:
		BVec m_value;
		Bit m_last;
	};

}
