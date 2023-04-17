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

		Counter(size_t end, size_t startupValue = 0) :
			m_area{ "scl_Counter", true }
		{
			if (!utils::isPow2(end)) {
				init(end, BitWidth::last(end), true, startupValue);
			}
			else {
				init(end, BitWidth::count(end), false, startupValue);
			}
			m_area.leave();
		}

		Counter(UInt end, size_t startupValue = 0) :
			m_area{ "scl_Counter", true}
		{
			init(end, end.width(), true, startupValue);

			m_area.leave();
		}
		
		Counter& inc() { m_inc = '1'; return *this; }

		void reset() { load(0); }
		const UInt& value() const { return m_value; }
		const Bit& isLast() const { return m_last; }
		Bit isFirst() const { return m_value == 0; }

		void load(UInt value) { m_load = '1'; m_loadValue = value; }
	protected:
		void init(UInt end, BitWidth counterW, bool checkOverflows, size_t resetValue = 0)
		{
			m_value = counterW;
			m_loadValue = counterW;

			HCL_NAMED(m_inc);

			m_last = m_value == (end - 1).lower(counterW);

			if (counterW != BitWidth(0)) {
				IF(m_inc)
				{
					m_value += 1;
					if (checkOverflows) {
						IF(m_last)
							m_value = 0;
					}
				}
			}
			HCL_NAMED(m_load);
			HCL_NAMED(m_loadValue);
			IF(m_load)
			{
				m_value = m_loadValue;
			}

			m_value = reg(m_value, resetValue);
			HCL_NAMED(m_value);
			HCL_NAMED(m_last);

			m_load = '0';
			m_inc = '0';
			m_loadValue = ConstUInt(m_loadValue.width());
		}


	private:
		Area m_area;

		UInt m_value;
		Bit m_last;

		UInt m_loadValue;
		Bit m_load;

		Bit m_inc;
	};

}
