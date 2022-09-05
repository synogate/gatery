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

#include "Area.h"
#include "Bit.h"
#include "BVec.h"
#include "BitWidth.h"
#include "ConditionalScope.h"
#include "Pack.h"
#include "Clock.h"
#include "SignalMiscOp.h"

#include "tech/TechnologyCapabilities.h"


#include <gatery/hlim/supportNodes/Node_Memory.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>
#include <gatery/hlim/NodePtr.h>


#include <functional>

namespace gtry 
{

	template<typename Data>
	class MemoryPortFactory {
	public:
		MemoryPortFactory(hlim::Node_Memory* memoryNode, const UInt& address, Data defaultValue) : m_memoryNode(memoryNode), m_defaultValue(defaultValue) {
			m_wordSize = width(m_defaultValue).value;

			size_t depth = (memoryNode->getSize() + m_wordSize-1) / m_wordSize;
			BitWidth expectedAddrBits = BitWidth::count(depth);

			if (address.width() < expectedAddrBits)
				m_address = zext(address, expectedAddrBits);
			else if (address.width() > expectedAddrBits)
				m_address = address.lower(expectedAddrBits);
			else
				m_address = address;
		}

		Data read() const
		{
			auto* readPort = DesignScope::createNode<hlim::Node_MemPort>(m_wordSize);
			readPort->connectMemory(m_memoryNode);
			if (auto* scope = gtry::ConditionalScope::get())
				readPort->connectEnable(scope->getFullCondition());

			readPort->connectAddress(m_address.readPort());
			readPort->setClock(ClockScope::getClk().getClk());

			UInt rawData(SignalReadPort({ .node = readPort, .port = (unsigned)hlim::Node_MemPort::Outputs::rdData }));
			Data ret = constructFrom(m_defaultValue);
			gtry::unpack(rawData, ret);
			return ret;
		}

		operator Data() const { return read(); }

		auto *write(const Data& value) {
			UInt packedValue = pack(value);

			HCL_DESIGNCHECK_HINT(packedValue.size() == m_wordSize, "The width of data assigned to a memory write port must match the previously specified word width of the memory or memory view.");

			auto* writePort = DesignScope::createNode<hlim::Node_MemPort>(m_wordSize);
			writePort->connectMemory(m_memoryNode);
			//writePort->connectEnable(constructEnableBit().readPort());
			if (auto* scope = gtry::ConditionalScope::get()) {
				writePort->connectEnable(scope->getFullCondition());
				writePort->connectWrEnable(scope->getFullCondition());
			}
			writePort->connectAddress(m_address.readPort());
			writePort->connectWrData(packedValue.readPort());
			writePort->setClock(ClockScope::getClk().getClk());
			return writePort;
		}

		MemoryPortFactory<Data>& operator = (const Data& value) { write(value); return *this; }

		void write(const Data& value, const UInt &wrWordEnable) {
			auto* writePort = write(value);
			writePort->connectWrWordEnable(wrWordEnable.readPort());
		}

		MemoryPortFactory<Data>& operator = (const std::pair<Data, UInt>& value) { write(value.first, value.second); return *this; }
	protected:
		hlim::NodePtr<hlim::Node_Memory> m_memoryNode;
		Data m_defaultValue;
		UInt m_address;
		std::size_t m_wordSize;

		Bit constructEnableBit() const {
			Bit enable;
			if (auto* scope = gtry::ConditionalScope::get())
				enable = gtry::SignalReadPort{ scope->getFullCondition() };
			else
				enable = '1';
			return enable;
		}
	};

	template<typename Data>
	Data reg(MemoryPortFactory<Data> readPort, const RegisterSettings& settings = {}) { return reg((Data)readPort, settings); }

	template<typename Data>
	void sim_tap(MemoryPortFactory<Data> readPort) { sim_tap((Data)readPort); }


	using MemType = hlim::Node_Memory::MemType;

	template<typename Data>
	class Memory 
	{
	public:
		Memory() = default;
		Memory(std::uint64_t numWords, Data def = Data{}) { setup(numWords, std::move(def)); }

		void setup(std::uint64_t numWords, Data def = Data{}) {

			HCL_DESIGNCHECK(m_memoryNode == nullptr);
			m_defaultValue = std::move(def);
			m_wordWidth = width(m_defaultValue).value;
			m_numWords = numWords;


			Area area{ "scl_memory" };
			auto ent = area.enter();

			ent["word_width"] = m_wordWidth;
			ent["num_words"] = numWords;
		
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

		size_t readLatencyHint() const { return m_memoryNode->getRequiredReadLatency(); }

		void setType(MemType type) { 
			if (type == MemType::EXTERNAL)
				m_memoryNode->setType(type, ~0ull);
			else {
				auto c = getTargetRequirementsForType(type);
				m_memoryNode->setType(type, c.totalReadLatency);
			}
		}
		void setType(MemType type, size_t readLatency) { 
			// if (type != MemType::EXTERNAL) {
			// 	auto c = getTargetRequirementsForType(type);
			// 	// TODO: This prevents things like: Set to DONT_CARE, choose a large size, and fore it into lutrams with a read latency of 0. Do we want that?
			// 	HCL_DESIGNCHECK_HINT(readLatency >= c.totalReadLatency, "The specified read latency is less than what the target device reports is necessary for this kind of memory!");
			// }
			m_memoryNode->setType(type, readLatency); 
		}

		MemoryCapabilities::Choice getTargetRequirements() const { return getTargetRequirementsForType(m_memoryNode->type()); }

		void setName(std::string name) { m_memoryNode->setName(std::move(name)); }
		void noConflicts() { m_memoryNode->setNoConflicts(); }
		bool valid() { return m_memoryNode != nullptr; }

		void fillPowerOnState(sim::DefaultBitVectorState powerOnState) { m_memoryNode->fillPowerOnState(std::move(powerOnState)); }
		void setPowerOnStateZero() {
			auto& state = m_memoryNode->getPowerOnState();
			state.clearRange(sim::DefaultConfig::VALUE, 0, state.size());
			state.setRange(sim::DefaultConfig::DEFINED, 0, state.size());
		}
		void addResetLogic(std::function<Data(UInt)> address2data) {
			m_memoryNode->setInitializationNetDataWidth(m_wordWidth);
			UInt data = pack(address2data(hlim::NodePort{m_memoryNode, (size_t)hlim::Node_Memory::Outputs::INITIALIZATION_ADDR}));
			m_memoryNode->rewireInput((size_t)hlim::Node_Memory::Inputs::INITIALIZATION_DATA, data.readPort());
		}

		void initZero() {
			setPowerOnStateZero();
			m_memoryNode->setInitializationNetDataWidth(m_wordWidth);
			UInt data = ConstUInt(0, BitWidth(m_wordWidth));
			m_memoryNode->rewireInput((size_t)hlim::Node_Memory::Inputs::INITIALIZATION_DATA, data.readPort());
		}

		std::size_t size() const { return m_memoryNode->getSize(); }
		BitWidth wordSize() const { return BitWidth{ m_wordWidth }; }
		BitWidth addressWidth() const { return BitWidth{ utils::Log2C(numWords()) }; }
		std::size_t numWords() const { return size() / m_wordWidth; }

		MemoryPortFactory<Data> operator [] (const UInt& address) { HCL_DESIGNCHECK(m_memoryNode != nullptr); return MemoryPortFactory<Data>(m_memoryNode, address, m_defaultValue); }
		const MemoryPortFactory<Data> operator [] (const UInt& address) const { HCL_DESIGNCHECK(m_memoryNode != nullptr); return MemoryPortFactory<Data>(m_memoryNode, address, m_defaultValue); }

		template<typename DataNew>
		Memory<DataNew> view(DataNew def = DataNew{}) { return Memory<DataNew>(m_memoryNode, def); }

		const Data& defaultValue() const { return m_defaultValue; }
	protected:
		hlim::NodePtr<hlim::Node_Memory> m_memoryNode;
		Data m_defaultValue;
		uint64_t m_numWords = 0;
		size_t m_wordWidth = 0;

		Memory(hlim::Node_Memory* memoryNode, Data def = Data{}) : m_memoryNode(memoryNode) { 
			m_defaultValue = std::move(def);
			m_wordWidth = width(m_defaultValue).value;
			m_numWords = m_memoryNode->getSize() / m_wordWidth;
			HCL_DESIGNCHECK(m_memoryNode->getSize() % m_wordWidth == 0);
		}

		MemoryCapabilities::Choice getTargetRequirementsForType(MemType type) const {
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
	};

	template<typename DataOld, typename DataNew>
	Memory<DataNew> view(Memory<DataOld> old, DataNew def = DataNew{}) { return old.view(def); }

}
