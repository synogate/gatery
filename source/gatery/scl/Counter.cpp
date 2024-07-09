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
#include <gatery/scl_pch.h>
#include "Counter.h"

namespace gtry::scl
{
	Counter::Counter(size_t end, size_t startupValue) :
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

	Counter::Counter(UInt end, size_t startupValue) :
		m_area{ "scl_Counter", true}
	{
		init(end, end.width(), true, startupValue);

		m_area.leave();
	}

	Counter::Counter(BitWidth ctrW, size_t startupValue) :
		m_area{ "scl_Counter", true}
	{
		init(ctrW.count(), ctrW, true, startupValue);

		m_area.leave();
	}

	Counter& Counter::inc() { 
		m_inc = '1';
		if (gtry::ConditionalScope ___condScope{'1', true})
			m_incrementNeverUsed &= '0';
		return *this;
	}

	Counter& Counter::dec() {
		m_dec = '1';
		if (gtry::ConditionalScope ___condScope{'1', true})
			m_incrementNeverUsed &= '0';
		return *this;
	}

	void Counter::reset() { load(m_resetValue); }
	void Counter::load(UInt value) { m_load = '1'; m_loadValue = value; }

	void Counter::init(UInt end, BitWidth counterW, bool checkOverflows, size_t resetValue)
	{
		m_resetValue = resetValue;
		m_value = counterW;
		m_loadValue = counterW;

		HCL_NAMED(m_inc);
		HCL_NAMED(m_dec);

		m_last = m_value == (end - 1).lower(counterW);

		if (counterW != BitWidth(0)) {
			UInt delta = ConstUInt(0, m_value.width());
			IF(m_incrementNeverUsed)
				delta = 1;    // +1, auto-increment by default
			IF(m_inc & !m_dec)
				delta = 1;    // +1
			IF(m_dec & !m_inc)
				delta |= '1'; // -1
			Bit isFirst = m_value == 0;
			m_value += delta;
			if (checkOverflows) {
				IF(delta == 1)
					IF(m_last)
					m_value = 0;

				IF(delta == delta.width().mask())
					IF(isFirst)
					m_value = (end - 1).lower(counterW);
			}
		}
		HCL_NAMED(m_load);
		HCL_NAMED(m_loadValue);
		IF(m_load)
		{
			m_value = m_loadValue;
		}
		m_becomesFirst = m_value == 0;
		m_value = reg(m_value, resetValue, {.allowRetimingBackward = true, .allowRetimingForward = true});
		HCL_NAMED(m_value);
		HCL_NAMED(m_last);

		m_load = '0';
		m_inc = '0';
		m_dec = '0';
		m_incrementNeverUsed = '1';
		m_loadValue = ConstUInt(m_loadValue.width());
	}

	UInt counterUpDown(Bit increment, Bit decrement, Bit reset, BitWidth ctrW, size_t resetValue) {
		Counter ctr(ctrW, resetValue);

		IF(increment)
			IF(!ctr.isLast())
				ctr.inc();

		IF(decrement)
			IF(!ctr.isFirst())
				ctr.dec();

		IF(reset)
			ctr.reset();

		return ctr.value();
	}
}
