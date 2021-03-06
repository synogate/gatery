#pragma once

#include <hcl/frontend.h>
#include "../algorithm/Stream.h"
#include "../Avalon.h"

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

	private:
		BitWidth m_hashWidth;
		BitWidth m_symbolWidth = 8_b;
		std::vector<Memory<BVec>> m_tables;
	};
}
