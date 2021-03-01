#include "TinyCuckoo.h"
#include "../crypto/SipHash.h"

#include <hcl/simulation/BitVectorState.h>

namespace hcl::stl
{
	TinyCuckooOut tinyCuckoo(const TinyCuckooIn& in, RegisterMap& mmap)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TinyCuckoo");

		TinyCuckooOut out;
		out.found = '0';
		out.hash = in.hash;
		out.key = in.key;
		out.userData = in.userData;
		out.value = zext(0, in.valueWidth().value);

		for(size_t l = 0; l < in.latency; l++)
			out = reg(out);

		for (size_t i = 0; i < in.numTables; ++i)
		{
			GroupScope entity(GroupScope::GroupType::ENTITY);
			entity.setName("TinyCuckooTable");

			Memory<TinyCuckooItem> mem(1ull << in.tableWidth().value, in.update.item);
			//mem.setType(MemType::BRAM);
			mem.setPowerOnState(
				hcl::core::sim::createDefaultBitVectorState(1ull << in.tableWidth().value, width(in.update.item), [](size_t idx, core::sim::DefaultConfig::BaseType* planes) {
					planes[core::sim::DefaultConfig::DEFINED] = ~0u;
					planes[core::sim::DefaultConfig::VALUE] = 0;
				}));

			IF(in.update.valid & in.update.tableIdx == i)
				mem[in.update.itemIdx] = in.update.item;

			SymbolSelect hashPart{ in.tableWidth().value };
			BVec lookupAddress = in.hash(hashPart[i]);
			HCL_NAMED(lookupAddress);

			TinyCuckooItem lookupData = mem[lookupAddress];
			for (size_t l = 0; l < in.latency; l++)
				lookupData = reg(lookupData);
			HCL_NAMED(lookupData);
			
			IF(lookupData.valid & lookupData.key == out.key)
			{
				out.found = '1';
				out.value = lookupData.value;
			}
		}

		HCL_NAMED(out);
		return out;
	}

}
