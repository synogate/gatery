#pragma once
#include "MemoryMap.h"

namespace hcl::stl
{
	class AvalonMMSlave : public MemoryMap
	{
	public:
		AvalonMMSlave(BitWidth addrWidth, BitWidth dataWidth);

		virtual void ro(const BVec& value, RegDesc desc);
		virtual void ro(const Bit& value, RegDesc desc);
		virtual Bit rw(BVec& value, RegDesc desc);
		virtual Bit rw(Bit& value, RegDesc desc);

		BVec address;
		Bit write;
		BVec writeData;
		BVec readData;

		std::vector<RegDesc> addressMap;
	};

	void pinIn(AvalonMMSlave& avmm, std::string prefix);
}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::AvalonMMSlave, address, write, writeData, readData);
