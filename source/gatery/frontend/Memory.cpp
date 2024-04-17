/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include "gatery/pch.h"

#include "Memory.h"
#include "Constant.h"


namespace gtry {

	void BaseMemory::setup()
	{
		Area area{ "scl_memory" };
		auto ent = area.enter();

		ent["word_width"] = m_wordWidth;
		ent["num_words"] = m_numWords;
	
		m_memoryNode = DesignScope::createNode<hlim::Node_Memory>();
		sim::DefaultBitVectorState state;
		state.resize(m_numWords * m_wordWidth);
		state.clearRange(sim::DefaultConfig::DEFINED, 0, m_numWords * m_wordWidth);
		m_memoryNode->setPowerOnState(std::move(state));

		MemType memType = ent.config("type").as(MemType::DONT_CARE);
		if (utils::ConfigTree lat = ent.config("readLatency"))
			setType(memType, lat.as<size_t>());
		else
			setType(memType);

		m_memoryNode->loadConfig();
	}

	size_t BaseMemory::readLatencyHint() const
	{
		return m_memoryNode->getRequiredReadLatency();
	}

	void BaseMemory::setType(MemType type)
	{ 
		if (type == MemType::EXTERNAL)
			m_memoryNode->setType(type, ~0ull);
		else {
			auto c = getTargetRequirementsForType(type);
			m_memoryNode->setType(type, c.totalReadLatency);
		}
	}

	void BaseMemory::setType(MemType type, size_t readLatency)
	{ 
		// if (type != MemType::EXTERNAL) {
		// 	auto c = getTargetRequirementsForType(type);
		// 	// TODO: This prevents things like: Set to DONT_CARE, choose a large size, and fore it into lutrams with a read latency of 0. Do we want that?
		// 	HCL_DESIGNCHECK_HINT(readLatency >= c.totalReadLatency, "The specified read latency is less than what the target device reports is necessary for this kind of memory!");
		// }
		m_memoryNode->setType(type, readLatency); 
	}

	MemoryCapabilities::Choice BaseMemory::getTargetRequirements() const
	{
		return getTargetRequirementsForType(m_memoryNode->type());
	}

	void BaseMemory::setName(std::string name)
	{
		m_memoryNode->setName(std::move(name));
	}
	
	const std::string &BaseMemory::name() const
	{
		return m_memoryNode->getName();
	}

	void BaseMemory::noConflicts()
	{
		m_memoryNode->setNoConflicts();
	}

	void BaseMemory::allowArbitraryPortRetiming()
	{
		m_memoryNode->allowArbitraryPortRetiming();
	}

	bool BaseMemory::valid()
	{
		return m_memoryNode != nullptr;
	}

	void BaseMemory::fillPowerOnState(sim::DefaultBitVectorState powerOnState)
	{
		m_memoryNode->fillPowerOnState(std::move(powerOnState));
	}

	void BaseMemory::setPowerOnStateZero()
	{
		auto& state = m_memoryNode->getPowerOnState();
		state.clearRange(sim::DefaultConfig::VALUE, 0, state.size());
		state.setRange(sim::DefaultConfig::DEFINED, 0, state.size());
	}

	void BaseMemory::initZero()
	{
		setPowerOnStateZero();
		m_memoryNode->setInitializationNetDataWidth(m_wordWidth);
		UInt data = ConstUInt(0, BitWidth(m_wordWidth));
		m_memoryNode->rewireInput((size_t)hlim::Node_Memory::Inputs::INITIALIZATION_DATA, data.readPort());
	}

	std::size_t BaseMemory::size() const
	{
		return m_memoryNode->getSize();
	}

	BitWidth BaseMemory::wordSize() const
	{
		return BitWidth{ m_wordWidth };
	}
	
	BitWidth BaseMemory::addressWidth() const
	{
		return BitWidth{ utils::Log2C(numWords()) };
	}

	std::size_t BaseMemory::numWords() const
	{
		return size() / m_wordWidth;
	}

	MemoryCapabilities::Choice BaseMemory::getTargetRequirementsForType(MemType type) const
	{
		MemoryCapabilities::Request request = {
			.size = m_memoryNode->getSize(),
		};

		if (m_memoryNode->getPorts().empty())
			request.maxDepth = m_numWords;
		else
			request.maxDepth = m_memoryNode->getMaxDepth();

		HCL_DESIGNCHECK_HINT(type != MemType::EXTERNAL, "Can't query the target device for properties of external memory!");
		switch (type) {
			case MemType::SMALL: request.sizeCategory = MemoryCapabilities::SizeCategory::SMALL; break;
			case MemType::MEDIUM: request.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM; break;
			case MemType::LARGE: request.sizeCategory = MemoryCapabilities::SizeCategory::LARGE; break;
			default: break;
		};
		return TechnologyScope::getCap<MemoryCapabilities>().select(request);
	}
}


