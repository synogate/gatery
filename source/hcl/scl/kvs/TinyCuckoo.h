/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include <hcl/frontend.h>
#include "../Avalon.h"
#include "../memoryMap/MemoryMap.h"

namespace hcl::stl
{
	template<typename Tkey, typename Tval>
	class TinyCuckoo
	{
	public:
		struct Out
		{
			BOOST_HANA_DEFINE_STRUCT(Out,
				(Bit, found),
				(Tval, value)
			);
		};

		struct Item
		{
			BOOST_HANA_DEFINE_STRUCT(Item,
				(Bit, valid),
				(Tkey, key),
				(Tval, value)
			);
		};

	public:
		TinyCuckoo(size_t capacity, const Tkey& key, const Tval& val, size_t numTables = 2);

		BitWidth hashWidth() const;
		size_t numTables() const { return m_tables.size(); }
		Out operator () (const Tkey& key, const BVec& hash);

		void addCpuInterface(AvalonNetworkSection& net);
		void addCpuInterface(MemoryMap& mmap);

	protected:
		std::vector<Memory<Item>> m_tables;

	};

	extern template class TinyCuckoo<BVec, BVec>;


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

	TinyCuckooOut tinyCuckoo(const TinyCuckooIn& in);

	template<typename Tkey, typename Tval>
	inline TinyCuckoo<Tkey, Tval>::TinyCuckoo(size_t capacity, const Tkey& key, const Tval& val, size_t numTables)
	{
		HCL_DESIGNCHECK(numTables);

		Item it{
			.valid = '0',
			.key = key,
			.value = val
		};

		m_tables.resize(numTables);
		for (Memory<Item>& mem : m_tables)
		{
			mem.setup(capacity / numTables, it);
			mem.setType(MemType::BRAM);
			mem.setPowerOnStateZero();
		}
	}

	template<typename Tkey, typename Tval>
	inline BitWidth TinyCuckoo<Tkey, Tval>::hashWidth() const
	{
		return m_tables[0].addressWidth() * m_tables.size();
	}

	template<typename Tkey, typename Tval>
	inline typename TinyCuckoo<Tkey, Tval>::Out TinyCuckoo<Tkey, Tval>::operator()(const Tkey& key, const BVec& hash)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TinyCuckoo_lookup");

		SymbolSelect hashSel{ hash.size() / m_tables.size() };

		Item item0 = m_tables[0][hash(hashSel[0])];
		HCL_NAMED(item0);

		Out ret;
		{
			GroupScope entity(GroupScope::GroupType::ENTITY);
			entity.setName("table");

			ret.value = item0.value;
			ret.found = item0.valid & item0.key == key;
			HCL_NAMED(ret);
		}

		for (size_t i = 1; i < m_tables.size(); ++i)
		{
			GroupScope entity(GroupScope::GroupType::ENTITY);
			entity.setName("table");

			Item item = m_tables[i][hash(hashSel[i])];
			HCL_NAMED(item);

			IF(item.valid & item.key == key)
			{
				ret.value = item.value;
				ret.found = '1';
			}
			HCL_NAMED(ret);
		}
		return ret;
	}

	template<typename Tkey, typename Tval>
	inline void TinyCuckoo<Tkey, Tval>::addCpuInterface(AvalonNetworkSection& net)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TinyCuckoo_addCpuInterface");

		for (size_t i = 0; i < m_tables.size(); ++i)
		{
			AvalonMM avmm;
			avmm.connect(m_tables[i]);
			net.add("table" + std::to_string(i), std::move(avmm));
		}
	}

	template<typename Tkey, typename Tval>
	inline void TinyCuckoo<Tkey, Tval>::addCpuInterface(MemoryMap& mmap)
	{
		mmap.stage(m_tables);
	}

}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooItem, valid, key, value);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooUpdate, valid, tableIdx, itemIdx, item);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooIn, key, hash, userData, update, numTables, latency);
BOOST_HANA_ADAPT_STRUCT(hcl::stl::TinyCuckooOut, found, key, hash, value, userData);
