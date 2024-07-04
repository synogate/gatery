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
		Counter(size_t end, size_t startupValue = 0);
		Counter(UInt end, size_t startupValue = 0);
		Counter(BitWidth ctrW, size_t startupValue = 0);

		Counter& inc();
		Counter& dec();
		void reset();
		void load(UInt value);

		inline const UInt& value() const { return m_value; }
		inline const Bit& isLast() const { return m_last; }
		inline Bit isFirst() const { return m_value == 0; }
		inline Bit becomesFirst() const { return m_becomesFirst; }
	protected:
		void init(UInt end, BitWidth counterW, bool checkOverflows, size_t resetValue = 0);

	private:
		Area m_area;

		UInt m_value;
		Bit m_last;
		Bit m_becomesFirst;

		UInt m_loadValue;
		size_t m_resetValue = 0;
		Bit m_load;

		Bit m_inc;
		Bit m_dec;

		Bit m_incrementNeverUsed;

	};

	UInt counterUpDown(Bit increment, Bit decrement, Bit reset, BitWidth ctrW, size_t resetValue = 0);
}
