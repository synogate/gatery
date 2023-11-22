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
#include "../stream/Stream.h"
#include "../Avalon.h"
#include "../memoryMap/MemoryMap.h"
#include "../memoryMap/MemoryMapConnectors.h"

namespace gtry::scl
{
	class TabulationHashing
	{
	public:
		TabulationHashing(BitWidth hashWidth = BitWidth{});

		TabulationHashing& hashWidth(BitWidth width);
		TabulationHashing& symbolWidth(BitWidth width);

		virtual UInt operator () (const UInt& data);
		size_t latency() const { return 1; }

		AvalonMM singleUpdatePort(bool readable = false);
		AvalonMM tableUpdatePort(size_t tableIdx, bool readable = false);

		void updatePorts(AvalonNetworkSection& net);

		void addCpuInterface(MemoryMap& mmap) { 
			mapIn(mmap, m_tables, "tabulation_hashing_tables"); 
			if (mmap.readEnabled())
				mapOut(mmap, m_tables, "tabulation_hashing_tables"); 
		}

		size_t numTables() const { return m_tables.size(); }
		BitWidth hashWidth() const { return m_hashWidth; }
		BitWidth symbolWidth() const { return m_symbolWidth; }

	private:
		BitWidth m_hashWidth;
		BitWidth m_symbolWidth = 8_b;
		std::vector<Memory<UInt>> m_tables;
	};
}
