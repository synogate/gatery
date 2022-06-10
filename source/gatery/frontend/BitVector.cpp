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

	static hlim::Node_Rewire::RewireOperation pickSelection(const BaseBitVector::Range& range)
	{
		hlim::Node_Rewire::RewireOperation op;
		op.addInput(0, range.offset, range.width);
		return op;
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
			parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	BaseBitVectorDefault::BaseBitVectorDefault(std::uint64_t value) {
		size_t width = utils::Log2C(value + 1);

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	BaseBitVectorDefault::BaseBitVectorDefault(std::string_view value) {
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBitVector(value),
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

	BaseBitVector::BaseBitVector(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) :
		m_node(node),
		m_range(range),
		m_expansionPolicy(expansionPolicy)
	{
		auto connType = node->getOutputConnectionType(0);
		HCL_DESIGNCHECK(connType.interpretation == hlim::ConnectionType::BITVEC);
		HCL_DESIGNCHECK(m_range.offset+m_range.width <= connType.width);

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
		assign(rhs.readPort());

		SignalReadPort outRange{ m_node, m_expansionPolicy };
		if (m_range.subset)
		{
			auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
			rewire->connectInput(0, outRange);
			rewire->setOp(pickSelection(m_range));
			outRange = SignalReadPort(rewire, m_expansionPolicy);
		}

		rhs.assign(outRange);
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

	BVec BaseBitVector::pack() const
	{
		if (m_node) 
			return BVec(readPort()); 
		else 
			return BVec{0_b};
	}

	void BaseBitVector::unpack(const BVec &b)
	{
		(*this) = b;
	}


	void BaseBitVector::resize(size_t width)
	{
		HCL_DESIGNCHECK_HINT(!m_range.subset, "BaseBitVector::resize is not allowed for alias BaseBitVector's. use zext instead.");
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
		m_range.width = width;
		m_bitAlias.clear();
	}

	Bit &BaseBitVector::getDynamicBitAlias(const UInt &idx) const
	{
		auto it = m_dynamicBitAlias.find(idx.readPort());
		if (it != m_dynamicBitAlias.end())	
			return it->second;

		// Force idx to undefined if out of range.
		UInt OoB_idx = idx;
		if ((1ull << OoB_idx.size())-1 >= m_range.width) // if we can represent more than allowed
			IF (OoB_idx >= m_range.width)
				OoB_idx = ConstUInt(OoB_idx.size());
		

		if (m_range.offsetDynamic.node != nullptr) {
			// If we are already a dynamic slice, we need to add both indices
			UInt currentOffset(SignalReadPort(m_range.offsetDynamic));
			UInt newOffset;
			if (currentOffset.size() > OoB_idx.size())
				newOffset = ext(currentOffset, 1) + ext(OoB_idx);
			else
				newOffset = ext(currentOffset) + ext(OoB_idx, 1);

			auto it2 = m_dynamicBitAlias.try_emplace(idx.readPort(), m_node, (hlim::NodePort)newOffset.readPort(), m_range.offset, m_range.maxDynamicIndex+m_range.width, m_initialScopeId);
			return it2.first->second;
		} else {
			auto it2 = m_dynamicBitAlias.try_emplace(idx.readPort(), m_node, (hlim::NodePort)OoB_idx.readPort(), m_range.offset, m_range.width, m_initialScopeId);
			return it2.first->second;
		}
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
		return hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BITVEC, .width = m_range.width };
	}

	SignalReadPort BaseBitVector::readPort() const
	{
		SignalReadPort driver = rawDriver();
		if (m_readPortDriver != driver.node)
		{
			m_readPort = driver;
			m_readPortDriver = driver.node;

			if (m_range.subset)
			{
				if (m_range.offsetDynamic.node != nullptr) {
					// Dynamic+static offset
					// Build a multiplexer that is fed with all possible different slices.

					hlim::ConnectionType idxType = hlim::getOutputConnectionType(m_range.offsetDynamic);

					HCL_ASSERT_HINT(idxType.interpretation == hlim::ConnectionType::BITVEC, "Index has wrong type");

					HCL_ASSERT(m_range.width > 0); /// @todo should we allow this?
					size_t fullOptions = m_range.maxDynamicIndex + 1;

					/// @todo do we allow dynamically slicing words that are partially out of bounds?
					auto *mux = DesignScope::createNode<hlim::Node_Multiplexer>(fullOptions);
					mux->connectSelector(m_range.offsetDynamic);

					for (auto i : utils::Range(fullOptions)) {
						auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
						rewire->connectInput(0, m_readPort);
						rewire->setExtract(m_range.offset + i, m_range.width);
						rewire->changeOutputType(connType());
						mux->connectInput(i, {.node=rewire, .port=0ull});
					}
					m_readPort = SignalReadPort(mux);
				} else {
					// Static only offset
					auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
					rewire->connectInput(0, m_readPort);
					rewire->setOp(pickSelection(m_range));
					m_readPort = SignalReadPort(rewire, m_expansionPolicy);
				}
			}
		}
		return m_readPort;
	}

	SignalReadPort BaseBitVector::outPort() const
	{
		SignalReadPort port{ m_node, m_expansionPolicy };

		if (m_range.subset)
		{
			auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
			rewire->connectInput(0, m_readPort);
			rewire->setOp(pickSelection(m_range));
			port = SignalReadPort(rewire, m_expansionPolicy);
		}
		return port;
	}

	std::string_view BaseBitVector::getName() const
	{
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
			parseBitVector(uint64_t(value), width),
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
			parseBitVector(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		assign(SignalReadPort(constant, policy));
	}

	void BaseBitVector::assign(std::string_view value, Expansion policy)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBitVector(value),
			hlim::ConnectionType::BITVEC
		);

		assign(SignalReadPort(constant, policy));
	}

	void BaseBitVector::assign(SignalReadPort in, bool ignoreConditions)
	{
		if (!m_node)
			createNode(in.width().bits(), in.expansionPolicy);

		const bool incrementWidth = in.width() > m_range.width;
		if(!incrementWidth)
			in = in.expand(m_range.width, hlim::ConnectionType::BITVEC);

		if (m_range.subset)
		{
			HCL_ASSERT(!incrementWidth);
			std::string in_name = in.node->getName();

			if (m_range.offsetDynamic.node != nullptr) {
				// Dynamic selection

				hlim::ConnectionType idxType = hlim::getOutputConnectionType(m_range.offsetDynamic);

				HCL_ASSERT_HINT(idxType.interpretation == hlim::ConnectionType::BITVEC, "Index has wrong type");

				auto *mux = DesignScope::createNode<hlim::Node_Multiplexer>(m_range.maxDynamicIndex+1);
				mux->connectSelector(m_range.offsetDynamic);

				for (auto i : utils::Range(m_range.maxDynamicIndex+1)) {
					auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
					rewire->connectInput(0, rawDriver());
					rewire->connectInput(1, in);
					rewire->setOp(replaceSelection(m_range.offset+i, m_range.width, m_node->getOutputConnectionType(0).width));
					rewire->changeOutputType(connType());
					mux->connectInput(i, {.node=rewire, .port=0ull});
				}
				in = SignalReadPort(mux);
			} else {
				// Static selection
				auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
				rewire->connectInput(0, rawDriver());
				rewire->connectInput(1, in);
				rewire->setOp(replaceSelection(m_range.offset, m_range.width, m_node->getOutputConnectionType(0).width));
				in = SignalReadPort(rewire);
			}
		}

		if (auto* scope = ConditionalScope::get(); !ignoreConditions && scope && scope->getId() > m_initialScopeId)
		{
			SignalReadPort oldSignal = rawDriver();


			if (incrementWidth)
			{
				HCL_ASSERT(m_expansionPolicy != Expansion::none);
				HCL_ASSERT(!m_range.subset);

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
				rewire->setExtract(0, m_range.width);

				for (const hlim::NodePort& port : nodeInputs)
					port.node->rewireInput(port.port, SignalReadPort{ rewire });

				HCL_ASSERT(!m_range.subset);
			}

			m_range.width = in.width().bits();
		}

		m_node->connectInput(in);
	}

	void BaseBitVector::createNode(size_t width, Expansion policy)
	{
		HCL_ASSERT(!m_node);

		m_range.width = width;
		m_expansionPolicy = policy;

		m_node = DesignScope::createNode<hlim::Node_Signal>();
		m_node->setConnectionType(connType());
		m_node->recordStackTrace();
	}

	SignalReadPort BaseBitVector::rawDriver() const
	{
		hlim::NodePort driver = m_node->getDriver(0);
		if (!driver.node)
			driver = hlim::NodePort{ .node = m_node, .port = 0ull };
		return SignalReadPort(driver, m_expansionPolicy);
	}

	std::vector<Bit>& BaseBitVector::aliasVec() const
	{
		if (m_bitAlias.size() != m_range.width)
		{
			m_bitAlias.reserve(m_range.width);
			for (size_t i = 0; i < m_range.width; ++i)
				if (m_range.offsetDynamic.node == nullptr)
					m_bitAlias.emplace_back(m_node, m_range.bitOffset(i), m_initialScopeId);
				else {
					// If the bit is taken from a slice with a dynamic offset, then this makes the bit selection dynamic.
					m_bitAlias.emplace_back(m_node, m_range.offsetDynamic, m_range.offset+i, m_range.maxDynamicIndex+1, m_initialScopeId);
				}
		}
		return m_bitAlias;
	}

	Bit& BaseBitVector::aliasMsb() const
	{
		if (!m_msbAlias)
		{
			if (!m_range.subset)
				m_msbAlias.emplace(m_node, ~0u, m_initialScopeId);
			else
				m_msbAlias.emplace(m_node, m_range.bitOffset(m_range.width - 1), m_initialScopeId);
		}
		return *m_msbAlias;
	}

	Bit& BaseBitVector::aliasLsb() const
	{
		if (!m_lsbAlias)
			m_lsbAlias.emplace(m_node, m_range.bitOffset(0), m_initialScopeId);
		return *m_lsbAlias;
	}


	BaseBitVector::Range::Range(const Selection& s, const Range& r)
	{
		if (s.start >= 0)
			offset = (size_t)s.start;
		else
		{
			offset = size_t(s.start + r.width);
		}

		if (s.untilEndOfSource) {
			width = r.width - offset;
		} else if (s.width >= 0)
			width = size_t(s.width);
		else
		{
			width = size_t(s.width + r.width);
		}

		offset += r.offset;
		subset = true;

		if (r.offsetDynamic.node != nullptr) {
			offsetDynamic = r.offsetDynamic;
			maxDynamicIndex = r.maxDynamicIndex;
		}

		HCL_DESIGNCHECK(offset+width <= r.offset+r.width);
		HCL_DESIGNCHECK(offset >= r.offset);
	}
	
	BaseBitVector::Range::Range(const UInt &dynamicOffset, BitWidth w, const Range& r) : Range(r)
	{
		subset = true;

		width = w.bits();
		HCL_DESIGNCHECK_HINT(w.bits() <= r.width, "Width of slice is larger than the width of the bitvector that is being sliced from.");
		// The max value for this index is such that it still fits fully within the parent slice.
		size_t maxThisDynamicIndex = r.width - w.bits();

		UInt idx = dynamicOffset;
		// Force idx to undefined if out of range.
		// For dynamic slices of dynamic slices, this needs to be done for each index in the chain individually
		if ((1ull << idx.size())-1 > maxThisDynamicIndex) // if we can represent more than allowed
			IF (idx > maxThisDynamicIndex)
				idx = ConstUInt(idx.size());
		
		if (r.offsetDynamic.node != nullptr) {
			UInt prevIdx(SignalReadPort(r.offsetDynamic));
			UInt finalIdx;
			if (prevIdx.size() > idx.size())
				finalIdx = ext(prevIdx, 1) + ext(idx);
			else
				finalIdx = ext(prevIdx) + ext(idx, 1);

			offsetDynamic.node = finalIdx.readPort().node;
			offsetDynamic.port = finalIdx.readPort().port;

			// The max index increases if the new slice is smaller than the previous slice.
			maxDynamicIndex = r.maxDynamicIndex + r.width - w.bits();
		} else {
			offsetDynamic.node = idx.readPort().node;
			offsetDynamic.port = idx.readPort().port;
			maxDynamicIndex = maxThisDynamicIndex;
		}
	}

	size_t BaseBitVector::getUIntBitWidth(const UInt &uint)
	{
		return uint.size();
	}

}
