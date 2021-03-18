#include "TabulationHashing.h"

namespace hcl::stl
{

	hcl::stl::TabulationHashing::TabulationHashing(BitWidth hashWidth) :
		m_hashWidth(hashWidth)
	{
	}

	TabulationHashing& hcl::stl::TabulationHashing::hashWidth(BitWidth width)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		m_hashWidth = width;
		return *this;
	}

	TabulationHashing& hcl::stl::TabulationHashing::symbolWidth(BitWidth width)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		m_symbolWidth = width;
		return *this;
	}

	BVec hcl::stl::TabulationHashing::operator()(const BVec& data)
	{
		HCL_ASSERT_HINT(m_tables.empty(), "invalid state");

		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TabulationHashing");

		const size_t numTables = (data.getWidth().value + m_symbolWidth.value - 1) / m_symbolWidth.value;
		m_tables.resize(numTables);

		BVec hash = zext(0, m_hashWidth.value);
		for (size_t t = 0; t < numTables; ++t)
		{
			const size_t addrWidth = std::min(m_symbolWidth.value, data.size() - t * m_symbolWidth.value);
			Memory<BVec>& table = m_tables[t];
			table.setup(1ull << addrWidth, m_hashWidth);

			const BVec& addr = data(t * m_symbolWidth.value, addrWidth);
			hash ^= table[addr];
		}
		HCL_NAMED(hash);
		return hash;
	}

	AvalonMM TabulationHashing::singleUpdatePort(bool readable)
	{
		HCL_ASSERT_HINT(!m_tables.empty(), "invalid state. call generator function first");
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TabulationHashing_UpdatePort");

		AvalonMM avmm;

		const Selection symbolAddrRange = Selection::Slice(0, (int)m_symbolWidth.value);
		const Selection tableAddrRange = Selection::Slice(symbolAddrRange.width, (int)utils::Log2C(m_tables.size()));
		avmm.addressSel["symbol"] = symbolAddrRange;
		avmm.addressSel["table"] = tableAddrRange;

		avmm.address = BitWidth{ uint32_t(tableAddrRange.start + tableAddrRange.width) };
		avmm.write = Bit{};
		avmm.writeData = m_hashWidth;

		if (readable)
			avmm.readData = ConstBVec(m_hashWidth.value);

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
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TabulationHashing_UpdatePort");

		AvalonMM avmm;
		avmm.connect(m_tables[tableIdx]);
		HCL_NAMED(avmm);
		return avmm;
	}
	
	void TabulationHashing::updatePorts(AvalonNetworkSection& net)
	{
		HCL_ASSERT_HINT(!m_tables.empty(), "invalid state. call generator function first");
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("TabulationHashing_Update");


		for (size_t i = 0; i < m_tables.size(); ++i)
		{
			std::string name = "table" + std::to_string(i);
			net.add(std::move(name), tableUpdatePort(i, true));
		}
	}
}
