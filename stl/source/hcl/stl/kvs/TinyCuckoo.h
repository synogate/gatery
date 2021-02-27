#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	class RegisterMap
	{
	public:
		struct RegDesc
		{
			std::string name;
			std::string desc;
		};

		virtual void ro(const BVec& value, RegDesc desc) {}
		virtual Bit rw(BVec& value, RegDesc desc) { return Bit{}; }
		virtual Bit rw(Bit& value, RegDesc desc) { return Bit{}; }
	};

	struct TinyCuckooItem
	{
		Bit valid;
		BVec key;
		BVec value;
	};

	struct TinyCuckooUpdate
	{
		Bit valid;
		BVec tableIdx;
		BVec itemIdx;
		TinyCuckooItem item;
	};

	struct TinyCuckooIn
	{
		BVec key;
		BVec hash;
		BVec userData;

		TinyCuckooUpdate update;

		size_t numTables = 2;
		size_t latency = 2;

		BitWidth valueWidth() const { return update.item.value.getWidth(); }
		BitWidth tableWidth() const { return update.itemIdx.getWidth(); }
	};


	struct TinyCuckooOut
	{
		Bit found;
		BVec key;
		BVec hash;
		BVec value;
		BVec userData;
	};

	TinyCuckooOut tinyCuckoo(const TinyCuckooIn& in, RegisterMap& mmap);

}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooItem, valid, key, value);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooUpdate, valid, tableIdx, itemIdx, item);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooIn, key, hash, userData, update, numTables, latency);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooOut, found, key, hash, value, userData);
