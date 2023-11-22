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
#include "../Avalon.h"
#include "../memoryMap/MemoryMap.h"
#include "../memoryMap/MemoryMapConnectors.h"

namespace gtry::scl
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
		Out operator () (const Tkey& key, const UInt& hash);

		void addCpuInterface(AvalonNetworkSection& net);
		void addCpuInterface(MemoryMap& mmap);

	protected:
		std::vector<Memory<Item>> m_tables;

	};

	extern template class TinyCuckoo<UInt, UInt>;


	struct TinyCuckooItem
	{
		Bit valid;
		UInt key;
		UInt value;
	};

	struct TinyCuckooUpdate
	{
		Bit valid;
		UInt tableIdx;
		UInt itemIdx;
		TinyCuckooItem item;
	};

	struct TinyCuckooIn
	{
		UInt key;
		UInt hash;
		UInt userData;

		TinyCuckooUpdate update;

		size_t numTables = 2;
		size_t latency = 2;

		BitWidth valueWidth() const { return update.item.value.width(); }
		BitWidth tableWidth() const { return update.itemIdx.width(); }
	};


	struct TinyCuckooOut
	{
		Bit found;
		UInt key;
		UInt hash;
		UInt value;
		UInt userData;
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
			mem.setType(MemType::MEDIUM);
			mem.initZero();
		}
	}

	template<typename Tkey, typename Tval>
	inline BitWidth TinyCuckoo<Tkey, Tval>::hashWidth() const
	{
		return m_tables[0].addressWidth() * m_tables.size();
	}

	template<typename Tkey, typename Tval>
	inline typename TinyCuckoo<Tkey, Tval>::Out TinyCuckoo<Tkey, Tval>::operator()(const Tkey& key, const UInt& hash)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "TinyCuckoo_lookup");

		SymbolSelect hashSel{ BitWidth{hash.size() / m_tables.size()} };

		Item item0 = m_tables[0][hash(hashSel[0])];
		HCL_NAMED(item0);

		Out ret;
		{
			GroupScope entity(GroupScope::GroupType::ENTITY, "table");

			ret.value = item0.value;
			ret.found = item0.valid & item0.key == key;
			HCL_NAMED(ret);
		}

		for (size_t i = 1; i < m_tables.size(); ++i)
		{
			GroupScope entity(GroupScope::GroupType::ENTITY, "table");

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
		GroupScope entity(GroupScope::GroupType::ENTITY, "TinyCuckoo_addCpuInterface");

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
		mapIn(mmap, m_tables, "cuckoo_tables"); 
		if (mmap.readEnabled())
			mapOut(mmap, m_tables, "cuckoo_tables"); 
	}

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TinyCuckooItem, valid, key, value);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TinyCuckooUpdate, valid, tableIdx, itemIdx, item);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TinyCuckooIn, key, hash, userData, update, numTables, latency);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TinyCuckooOut, found, key, hash, value, userData);
