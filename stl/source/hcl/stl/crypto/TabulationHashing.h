#pragma once

#include <hcl/frontend.h>
#include "../Stream.h"
#include "../Avalon.h"
#include "../memoryMap/MemoryMap.h"

namespace hcl::stl
{
	class TabulationHashing
	{
	public:
		TabulationHashing(BitWidth hashWidth = BitWidth{});

		TabulationHashing& hashWidth(BitWidth width);
		TabulationHashing& symbolWidth(BitWidth width);

		virtual BVec operator () (const BVec& data);
		size_t latency() const { return 1; }

		AvalonMM singleUpdatePort(bool readable = false);
		AvalonMM tableUpdatePort(size_t tableIdx, bool readable = false);

		void updatePorts(AvalonNetworkSection& net);

		void addCpuInterface(MemoryMap& mmap) { mmap.stage(m_tables); }

		size_t numTables() const { return m_tables.size(); }
		BitWidth hashWidth() const { return m_hashWidth; }
		BitWidth symbolWidth() const { return m_symbolWidth; }

	private:
		BitWidth m_hashWidth;
		BitWidth m_symbolWidth = 8_b;
		std::vector<Memory<BVec>> m_tables;
	};
}
