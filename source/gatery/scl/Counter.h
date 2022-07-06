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
	class Counter
	{
	public:
		Counter(size_t end) :
			m_value{BitWidth{utils::Log2C(end)}}
		{
			m_last = '0';

			IF(m_value == end - 1)
			{
				m_value = 0;
				m_last = '1';
			}
			ELSE
			{
				m_value = m_value + 1;
			}
			m_value = reg(m_value, 0);
		}
		
		void reset() { m_value = 0; }
		const UInt& value() const { return m_value; }
		const Bit& isLast() const { return m_last; }
		Bit isFirst() const { return m_value == 0; }

	private:
		UInt m_value;
		Bit m_last;
	};

}
