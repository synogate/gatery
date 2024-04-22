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
#include "gatery/pch.h"
#include "BitVector.h"
#include "ConditionalScope.h"
#include "Constant.h"
#include "UInt.h"
#include "ConditionalScope.h"
#include "SignalArithmeticOp.h"
#include "SignalCompareOp.h"

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/supportNodes/Node_Default.h>

namespace gtry {

	Selection Selection::All()
	{
		return { .untilEndOfSource = true };
	}

	Selection Selection::From(int start)
	{
		return {
			.start = start,
			.width = 0,
			.untilEndOfSource = true,
		};
	}

	Selection Selection::Range(int start, int end)
	{
		return {
			.start = start,
			.width = end - start,
			.untilEndOfSource = false,
		};
	}

	Selection Selection::Range(size_t start, size_t end)
	{
		return {
			.start = (int)start,
			.width = (int)(end - start),
			.untilEndOfSource = false,
		};
	}
	Selection Selection::RangeIncl(int start, int endIncl)
	{
		return {
			.start = start,
			.width = endIncl - start + 1,
			.untilEndOfSource = false,
		};
	}

	Selection Selection::Slice(size_t offset, size_t size)
	{
		return {
			.start = (int)offset,
			.width = (int)size,
			.untilEndOfSource = false,
		};
	}

	Selection Selection::Symbol(int idx, BitWidth symbolWidth)
	{
		return {
			.start = idx * int(symbolWidth.value),
			.width = int(symbolWidth.value),
			.untilEndOfSource = false
		};
	}

	static hlim::Node_Rewire::RewireOperation replaceSelection(size_t rangeOffset, size_t rangeWidth, size_t totalWidth)
	{
		HCL_ASSERT(rangeOffset+rangeWidth <= totalWidth);

		hlim::Node_Rewire::RewireOperation op;

		op.addInput(0, 0, rangeOffset);
		op.addInput(1, 0, rangeWidth);
		op.addInput(0, rangeOffset + rangeWidth, totalWidth - (rangeOffset + rangeWidth));

		return op;
	}

	BaseBitVectorDefault::BaseBitVectorDefault(const BaseBitVector& rhs) {
		m_nodePort = rhs.readPort();
	}

	BaseBitVectorDefault::BaseBitVectorDefault(std::int64_t value) {
		size_t width = utils::Log2C(std::abs(value) + 1) + 1;

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	BaseBitVectorDefault::BaseBitVectorDefault(std::uint64_t value) {
		size_t width = utils::Log2C(value + 1);

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	BaseBitVectorDefault::BaseBitVectorDefault(std::string_view value) {
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(value),
			hlim::ConnectionType::BITVEC
		);

		m_nodePort = {.node = constant, .port = 0ull };
	}


	BaseBitVector::BaseBitVector(BaseBitVector&& rhs) :
		ElementarySignal()
	{
		if (rhs.m_node)
		{
			assign(rhs.readPort());
			rhs.assign(SignalReadPort(m_node, rhs.m_expansionPolicy));
		}
	}

	BaseBitVector::BaseBitVector(hlim::Node_Signal* node, std::shared_ptr<BitVectorSlice> range, Expansion expansionPolicy, size_t initialScopeId) :
		m_node(node),
		m_range(move(range)),
		m_width(m_range->width()),
		m_expansionPolicy(expansionPolicy)
	{
		HCL_DESIGNCHECK(m_range);
		m_initialScopeId = initialScopeId;
	}

	BaseBitVector::BaseBitVector(BitWidth width, Expansion expansionPolicy)
	{
		createNode(width.value, expansionPolicy);
	}

	BaseBitVector::BaseBitVector(const BaseBitVectorDefault &defaultValue)
	{
		(*this) = defaultValue;
	}

	BaseBitVector& BaseBitVector::operator=(BaseBitVector&& rhs)
	{
		auto port = rhs.node()->getDriver(0);
		if (port.node == nullptr)
		{
			// special case where we move a unassigned signal into an existing signal
			// an implementation with conditional scopes is possibile but has many corner cases
			// think of assigning a signal that is only conditionally loopy
			HCL_ASSERT_HINT(!m_node || ConditionalScope::get() == nullptr, "no impl");

			resetNode();
			createNode(rhs.size(), rhs.m_expansionPolicy);
		}
		else
		{
			assign(rhs.readPort());
		}
		rhs.assign(outPort());
		return *this;
	}

	BaseBitVector& BaseBitVector::operator=(BitWidth width)
	{
		HCL_DESIGNCHECK(!m_node);
		createNode(width.value, m_expansionPolicy);
		return *this;
	}

	BaseBitVector& BaseBitVector::operator=(const BaseBitVectorDefault &defaultValue)
	{
		hlim::Node_Default* node = DesignScope::createNode<hlim::Node_Default>();
		node->recordStackTrace();
		node->connectInput(readPort());
		node->connectDefault(defaultValue.getNodePort());

		assign(SignalReadPort(node));
		return *this;
	}

	void BaseBitVector::exportOverride(const BaseBitVector& exportOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(readPort());
		expOverride->connectOverride(exportOverride.readPort());
		assign(SignalReadPort(expOverride));
	}

	void BaseBitVector::simulationOverride(const BaseBitVector& simulationOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(simulationOverride.readPort());
		expOverride->connectOverride(readPort());
		assign(SignalReadPort(expOverride));
	}

	void BaseBitVector::resize(size_t width)
	{
		HCL_DESIGNCHECK_HINT(!m_range, "BaseBitVector::resize is not allowed for alias BaseBitVector's. use zext instead.");
		HCL_DESIGNCHECK_HINT(m_node->getDirectlyDriven(0).empty(), "BaseBitVector::resize is allowed for unused signals (final)");
		HCL_DESIGNCHECK_HINT(width > size(), "BaseBitVector::resize width decrease not allowed");
		HCL_DESIGNCHECK_HINT(width <= size() || m_expansionPolicy != Expansion::none, "BaseBitVector::resize width increase only allowed when expansion policy is set");

		if (width == size())
			return;

		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->connectInput(0, readPort());

		switch (m_expansionPolicy)
		{
		case Expansion::sign: rewire->setPadTo(width); break;
		case Expansion::zero: rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ZERO); break;
		case Expansion::one: rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ONE); break;
		default: break;
		}

		m_node->connectInput({ .node = rewire, .port = 0 }); // unconditional (largest of all paths wins)
		m_width = BitWidth{ width };
		m_bitAlias.clear();
	}

	void gtry::BaseBitVector::resetNode()
	{
		HCL_DESIGNCHECK_HINT(!m_range, "BaseBitVector::resetNode is not allowed for alias BaseBitVector's.");
		m_node = nullptr;
		m_range.reset();
		m_width = 0_b;
		m_expansionPolicy = Expansion::none;
		m_bitAlias.clear();
		m_lsbAlias = std::nullopt;
		m_msbAlias = std::nullopt;
		m_dynamicBitAlias.clear();
		m_readPort = {};
		m_readPortDriver = nullptr;

		if (auto* scope = ConditionalScope::get())
			m_initialScopeId = scope->getId();		
	}

	Bit &BaseBitVector::getDynamicBitAlias(const UInt &idx) const
	{
		auto it = m_dynamicBitAlias.find(idx.readPort());
		if (it != m_dynamicBitAlias.end())	
			return it->second;
		// cast size_t to uint64_t because clang interprets size_t differently than gcc
		auto slice = std::make_shared<BitVectorSliceDynamic>(idx, std::min((uint64_t)size() - 1, idx.width().last()), 1, 1_b, m_range);

		auto it2 = m_dynamicBitAlias.try_emplace(idx.readPort(), 
			m_node, slice, m_initialScopeId
		);
		return it2.first->second;
	}

	Bit& BaseBitVector::operator[](const UInt &idx)
	{
		return getDynamicBitAlias(idx);
	}

	const Bit& BaseBitVector::operator[](const UInt &idx) const
	{
		return getDynamicBitAlias(idx);
	}

	hlim::ConnectionType BaseBitVector::connType() const
	{
		return hlim::ConnectionType{ .type = hlim::ConnectionType::BITVEC, .width = width().bits() };
	}

	SignalReadPort BaseBitVector::readPort() const
	{
		SignalReadPort driver = rawDriver();
		if (m_readPortDriver != driver.node)
		{
			m_readPort = driver;
			m_readPortDriver = driver.node;

			if (m_range)
			{
				m_readPort = m_range->readPort(driver);
				m_readPort.expansionPolicy = m_expansionPolicy;
			}
		}
		return m_readPort;
	}

	SignalReadPort BaseBitVector::outPort() const
	{
		SignalReadPort port{ m_node };
		if (m_range)
			port = m_range->readPort(port);
		port.expansionPolicy = m_expansionPolicy;
		return port;
	}

	std::string_view BaseBitVector::getName() const
	{
		if (m_node == nullptr) { return {}; }
		if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
			return sigNode->getName();
		return {};
	}

	void BaseBitVector::setName(std::string name)
	{
		HCL_DESIGNCHECK_HINT(m_node != nullptr, "Can only set names to initialized BaseBitVectors!");

		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();

		assign(SignalReadPort(signal), true);
	}

	void BaseBitVector::setName(std::string name) const
	{
		HCL_DESIGNCHECK_HINT(m_node != nullptr, "Can only set names to initialized BaseBitVectors!");

		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();
	}

	void BaseBitVector::addToSignalGroup(hlim::SignalGroup *signalGroup)
	{
		m_node->moveToSignalGroup(signalGroup);
	}


	void BaseBitVector::assign(std::uint64_t value, Expansion policy)
	{
		size_t width;
		if (value + 1 == 0)
			width = sizeof(value)*8; // Handle special case of overflow in utils::Log2C(value + 1)
		else
			width = utils::Log2C(value + 1);

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		assign(SignalReadPort(constant, policy));
	}

	void BaseBitVector::assign(std::int64_t value, Expansion policy)
	{
		size_t width;
		if (value >= 0)
			width = utils::Log2C(value + 1)+1;
		else
			width = utils::Log2C(~value + 1)+1;

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		assign(SignalReadPort(constant, policy));
	}

	void BaseBitVector::assign(std::string_view value, Expansion policy)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(value),
			hlim::ConnectionType::BITVEC
		);

		assign(SignalReadPort(constant, policy));
	}

	void BaseBitVector::assign(SignalReadPort in, bool ignoreConditions)
	{
		if (!m_node)
			createNode(in.width().bits(), in.expansionPolicy);

		const bool incrementWidth = in.width().bits() > width().bits();
		
		HCL_DESIGNCHECK_HINT(m_node->getDirectlyDriven(0).empty() || !incrementWidth, "Can not increase width of a bit vector that is already driving signals.");

		if(!incrementWidth)
			in = in.expand(width().bits(), hlim::ConnectionType::BITVEC);

		if (m_range)
		{
			HCL_ASSERT(!incrementWidth);
			in = m_range->assign(rawDriver(), in);
		}

		if (auto* scope = ConditionalScope::get(); !ignoreConditions && scope && scope->getId() > m_initialScopeId)
		{
			SignalReadPort oldSignal = rawDriver();


			if (incrementWidth)
			{
				HCL_ASSERT(m_expansionPolicy != Expansion::none);
				HCL_ASSERT(!m_range);

				auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
				rewire->connectInput(0, oldSignal);

				switch (m_expansionPolicy)
				{
				case Expansion::zero:   rewire->setPadTo(in.width().bits(), hlim::Node_Rewire::OutputRange::CONST_ZERO);	break;
				case Expansion::one:	rewire->setPadTo(in.width().bits(), hlim::Node_Rewire::OutputRange::CONST_ONE);	break;
				case Expansion::sign:   rewire->setPadTo(in.width().bits()); break;
				default: break;
				}

				oldSignal = SignalReadPort{ rewire };
			}

			auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
			mux->connectInput(0, oldSignal);
			mux->connectInput(1, in); // assign rhs last in case previous port was undefined
			mux->connectSelector(scope->getFullCondition());
			mux->setConditionId(scope->getId());
			in = SignalReadPort{ mux };
		}

		if (incrementWidth)
		{
			if (const auto nodeInputs = m_node->getDirectlyDriven(0); !nodeInputs.empty())
			{
				auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
				rewire->connectInput(0, SignalReadPort{ m_node });
				rewire->setExtract(0, width().bits());

				for (const hlim::NodePort& port : nodeInputs)
					port.node->rewireInput(port.port, SignalReadPort{ rewire });

				HCL_ASSERT(!m_range);
			}

			m_width = in.width();
		}

		m_node->connectInput(in);
	}

	void BaseBitVector::createNode(size_t width, Expansion policy)
	{
		HCL_ASSERT(!m_node);

		m_width = BitWidth{ width };
		m_expansionPolicy = policy;

		m_node = DesignScope::createNode<hlim::Node_Signal>();
		m_node->setConnectionType(connType());
		m_node->recordStackTrace();
	}

	SignalReadPort BaseBitVector::rawDriver() const
	{
		HCL_ASSERT_HINT(valid(), "This signal has not been initialized with a BitWidth");
		hlim::NodePort driver = m_node->getDriver(0);
		if (!driver.node)
			driver = hlim::NodePort{ .node = m_node, .port = 0ull };
		return SignalReadPort(driver, m_expansionPolicy);
	}

	std::vector<Bit>& BaseBitVector::aliasVec() const
	{
		if (m_bitAlias.size() != width().bits())
		{
			m_bitAlias.reserve(width().bits());
			for (size_t i = 0; i < width().bits(); ++i)
			{
				auto slice = std::make_shared<BitVectorSliceStatic>(Selection::Slice(i, 1), width(), m_range);
				m_bitAlias.emplace_back(m_node, slice, m_initialScopeId);
			}
		}
		return m_bitAlias;
	}

	Bit& BaseBitVector::aliasMsb() const
	{
		if (!m_msbAlias)
		{
			HCL_DESIGNCHECK(width() != 0_b);
			auto slice = std::make_shared<BitVectorSliceStatic>(Selection::Slice(width().bits() - 1, 1), width(), m_range);
			m_msbAlias.emplace(m_node, slice, m_initialScopeId);
		}
		return *m_msbAlias;
	}

	Bit& BaseBitVector::aliasLsb() const
	{
		if (!m_lsbAlias)
		{
			HCL_DESIGNCHECK(width() != 0_b);
			auto slice = std::make_shared<BitVectorSliceStatic>(Selection::Slice(0, 1), width(), m_range);
			m_lsbAlias.emplace(m_node, slice, m_initialScopeId);
		}
		return *m_lsbAlias;
	}

	size_t BaseBitVector::getUIntBitWidth(const UInt &uint)
	{
		return uint.size();
	}

}
