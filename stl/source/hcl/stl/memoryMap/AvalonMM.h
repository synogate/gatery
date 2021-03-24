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

		virtual void enterScope(std::string scope) override;
		virtual void leaveScope() override;

		BVec address;
		Bit write;
		BVec writeData;
		BVec readData;

		std::vector<RegDesc> addressMap;

		protected:
			std::vector<std::string> scopeStack;
			void addRegDescChunk(const RegDesc &desc, size_t offset, size_t width);
	};

	void pinIn(AvalonMMSlave& avmm, std::string prefix);
}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::AvalonMMSlave, address, write, writeData, readData);
