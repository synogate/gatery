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
#include "gatery/scl_pch.h"
#include "TabulationHashing.h"

namespace gtry::scl
{

	gtry::scl::TabulationHashing::TabulationHashing(BitWidth hashWidth) :
		m_hashWidth(hashWidth)
	{
	}

	TabulationHashing& gtry::scl::TabulationHashing::hashWidth(BitWidth width)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		m_hashWidth = width;
		return *this;
	}

	TabulationHashing& gtry::scl::TabulationHashing::symbolWidth(BitWidth width)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		m_symbolWidth = width;
		return *this;
	}

	UInt gtry::scl::TabulationHashing::operator()(const UInt& data)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		GroupScope entity(GroupScope::GroupType::ENTITY, "TabulationHashing");

		const size_t numTables = (data.width().value + m_symbolWidth.value - 1) / m_symbolWidth.value;
		m_tables.resize(numTables);

		UInt hash = zext(0, m_hashWidth);
		for (size_t t = 0; t < numTables; ++t)
		{
			const BitWidth addrWidth = std::min(m_symbolWidth, data.width() - t * m_symbolWidth);
			Memory<UInt>& table = m_tables[t];
			table.setup(addrWidth.count(), m_hashWidth);

			const UInt& addr = data(t * m_symbolWidth.value, addrWidth);
			hash ^= table[addr];
		}
		HCL_NAMED(hash);
		return hash;
	}

	AvalonMM TabulationHashing::singleUpdatePort(bool readable)
	{
		HCL_ASSERT_HINT(!m_tables.empty(), "invalid state. call generator function first");
		GroupScope entity(GroupScope::GroupType::ENTITY, "TabulationHashing_UpdatePort");

		AvalonMM avmm;

		const Selection symbolAddrRange = Selection::Slice(0, m_symbolWidth.value);
		const Selection tableAddrRange = Selection::Slice(symbolAddrRange.width, utils::Log2C(m_tables.size()));
		avmm.addressSel["symbol"] = symbolAddrRange;
		avmm.addressSel["table"] = tableAddrRange;

		avmm.address = BitWidth{ uint32_t(tableAddrRange.start + tableAddrRange.width) };
		avmm.write = Bit{};
		avmm.writeData = m_hashWidth;

		if (readable)
			avmm.readData = m_hashWidth;

		for (size_t t = 0; t < m_tables.size(); ++t)
		{
			IF(avmm.address(tableAddrRange) == t)
			{
				auto port = m_tables[t][avmm.address(symbolAddrRange)];

				IF(*avmm.write)
					port = *avmm.writeData;

				if (avmm.readData)
					*avmm.readData = port;
			}
		}
		HCL_NAMED(avmm);
		return avmm;
	}

	AvalonMM TabulationHashing::tableUpdatePort(size_t tableIdx, bool readable)
	{
		HCL_ASSERT_HINT(!m_tables.empty(), "invalid state. call generator function first");
		HCL_ASSERT(tableIdx < m_tables.size());
		GroupScope entity(GroupScope::GroupType::ENTITY, "TabulationHashing_UpdatePort");

		AvalonMM avmm;
		avmm.connect(m_tables[tableIdx]);
		HCL_NAMED(avmm);
		return avmm;
	}
	
	void TabulationHashing::updatePorts(AvalonNetworkSection& net)
	{
		HCL_ASSERT_HINT(!m_tables.empty(), "invalid state. call generator function first");
		GroupScope entity(GroupScope::GroupType::ENTITY, "TabulationHashing_Update");

		for (size_t i = 0; i < m_tables.size(); ++i)
		{
			std::string name = "table" + std::to_string(i);
			net.add(std::move(name), tableUpdatePort(i, true));
		}
	}
}
