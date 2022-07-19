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
#include "gatery/pch.h"

#include "Bit.h"
#include "BVec.h"
#include "ConditionalScope.h"
#include "Constant.h"
#include "DesignScope.h"
#include "PipeBalanceGroup.h"
#include "Reg.h"
#include "Scope.h"
#include "SignalCompareOp.h"

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_Default.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/supportNodes/Node_Attributes.h>
#include <gatery/hlim/supportNodes/Node_RegHint.h>


namespace gtry {

	BitDefault::BitDefault(const Bit& rhs)
	{
		m_nodePort = rhs.readPort();
	}

	void BitDefault::assign(bool value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBit(value), hlim::ConnectionType::BOOL
			);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	void BitDefault::assign(char value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBit(value), hlim::ConnectionType::BOOL
			);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	Bit reg(const Bit& val, const RegisterSettings& settings)
	{
		if(auto rval = val.resetValue())
			return reg<Bit>(val, *rval, settings);

		return reg<Bit>(val, settings);
	}

	Bit reg(const Bit& val)
	{
		return reg(val, RegisterSettings{});
	}

	Bit::Bit()
	{
		createNode();
	}

	Bit::Bit(const Bit& rhs) : Bit(rhs.readPort())
	{
		m_resetValue = rhs.m_resetValue;
	}

	Bit::Bit(const Bit& rhs, construct_from_t&&) : Bit()
	{
		m_resetValue = rhs.m_resetValue;
	}

	Bit::Bit(Bit&& rhs) : Bit()
	{
		// TODO optimize, make simple alias move if src is not in a conditional
		assign(rhs.readPort());
		rhs.assign(SignalReadPort{ m_node });
		m_resetValue = rhs.m_resetValue;
	}

	Bit::Bit(const BitDefault &defaultValue) : Bit()
	{
		(*this) = defaultValue;
	} 

	gtry::Bit::Bit(const SignalReadPort& port, std::optional<bool> resetValue) :
		m_resetValue(resetValue)
	{
		createNode();
		if(port.node)
			m_node->connectInput(port);
	}

	Bit::Bit(hlim::Node_Signal* node, size_t offset, size_t initialScopeId) :
		m_node(node),
		m_offset(offset)
	{
		m_initialScopeId = initialScopeId;
	}

	Bit::Bit(hlim::Node_Signal* node, hlim::NodePort idx, size_t dynRangeOffset, size_t dynRangeWidth, size_t initialScopeId) :
		m_node(node),
		m_offsetDynamic(idx),
		m_dynRangeOffset(dynRangeOffset),
		m_dynRangeWidth(dynRangeWidth)
	{
		m_initialScopeId = initialScopeId;
	}

	Bit& Bit::operator=(Bit&& rhs)
	{
		if(rhs.m_resetValue)
			m_resetValue = rhs.m_resetValue;

		assign(rhs.readPort());

		SignalReadPort outRange{ m_node };
		outRange = rewireAlias(outRange);
		rhs.assign(outRange);
		return *this;
	}

	Bit& Bit::operator=(const BitDefault &defaultValue)
	{
		hlim::Node_Default* node = DesignScope::createNode<hlim::Node_Default>();
		node->recordStackTrace();
		node->connectInput(readPort());
		node->connectDefault(defaultValue.getNodePort());

		assign(SignalReadPort(node));
		return *this;
	}

	void Bit::exportOverride(const Bit& exportOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(readPort());
		expOverride->connectOverride(exportOverride.readPort());
		assign(SignalReadPort(expOverride));
	}

	BitWidth Bit::width() const
	{
		return 1_b;
	}

	hlim::ConnectionType Bit::connType() const
	{
		return hlim::ConnectionType{
			.interpretation = hlim::ConnectionType::BOOL,
			.width = 1
		};
	}

	SignalReadPort Bit::readPort() const
	{
		SignalReadPort ret = rawDriver();
		return rewireAlias(ret);
	}

	SignalReadPort Bit::outPort() const
	{
		SignalReadPort ret{ m_node };
		return rewireAlias(ret);
	}

	Bit Bit::final() const
	{
		return rewireAlias(SignalReadPort{ m_node });
	}

	SignalReadPort gtry::Bit::rewireAlias(SignalReadPort port) const
	{
		hlim::ConnectionType type = hlim::getOutputConnectionType(port);
		if (type.interpretation != hlim::ConnectionType::BOOL)
		{
			if (m_offsetDynamic.node != nullptr) {
				// Dynamic selection
				hlim::ConnectionType idxType = hlim::getOutputConnectionType(m_offsetDynamic);

				HCL_ASSERT_HINT(idxType.interpretation == hlim::ConnectionType::BITVEC, "Index has wrong type");

				auto *mux = DesignScope::createNode<hlim::Node_Multiplexer>(m_dynRangeWidth);
				mux->connectSelector(m_offsetDynamic);

				for (auto i : utils::Range(m_dynRangeWidth)) {
					auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
					rewire->connectInput(0, port);
					rewire->setExtract(m_dynRangeOffset + i, 1);
					rewire->changeOutputType(connType());
					mux->connectInput(i, {.node=rewire, .port=0ull});
				}
				port = SignalReadPort(mux);
			} else {
				// Static selection

				// TODO: cache rewire node if m_node's input is unchanged
				auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
				rewire->connectInput(0, port);
				rewire->changeOutputType(connType());

				size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
				rewire->setExtract(offset, 1);
				port = SignalReadPort(rewire);
			}
		}
		return port;
	}

	std::string_view Bit::getName() const
	{
		if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
			return sigNode->getName();
		return {};
	}

	void Bit::setName(std::string name)
	{
		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();

		assign(SignalReadPort(signal), true);
	}

	void Bit::addToSignalGroup(hlim::SignalGroup *signalGroup)
	{
		m_node->moveToSignalGroup(signalGroup);
	}

	void Bit::resetValue(bool v)
	{
		m_resetValue = v;
	}

	void Bit::resetValue(char v)
	{
		HCL_ASSERT(v == '1' || v == '0'); resetValue(v == '1');
	}

	void Bit::createNode()
	{
		HCL_ASSERT(!m_node);
		m_node = DesignScope::createNode<hlim::Node_Signal>();
		m_node->setConnectionType(connType());
		m_node->recordStackTrace();
	}

	void Bit::assign(bool value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBit(value), hlim::ConnectionType::BOOL
			);
		assign(SignalReadPort(constant));
	}

	void Bit::assign(char value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBit(value), hlim::ConnectionType::BOOL
			);
		assign(SignalReadPort(constant));
	}

	void Bit::assign(SignalReadPort in, bool ignoreConditions)
	{
		hlim::ConnectionType type = m_node->getOutputConnectionType(0);

		if (type.interpretation != hlim::ConnectionType::BOOL)
		{
			if (m_offsetDynamic.node != nullptr) {
				// Dynamic selection

				// first, mux individual bits and fuse
				auto* fuseRewire = DesignScope::createNode<hlim::Node_Rewire>(m_dynRangeWidth);
				for (auto i : utils::Range(m_dynRangeWidth)) {
					// extract
					auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
					rewire->connectInput(0, rawDriver());
					rewire->setExtract(m_dynRangeOffset + i, 1);
					rewire->changeOutputType(connType());

					// mux
					auto selectorBit = UInt(SignalReadPort(m_offsetDynamic)) == i;
					
					auto *mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
					mux->connectSelector(selectorBit.readPort());
					mux->connectInput(0, {.node=rewire, .port=0ull});
					mux->connectInput(1, in);

					// merge
					fuseRewire->connectInput(i, {.node=mux, .port=0ull});
				}
				fuseRewire->setConcat();


				// second, if m_offsetDynamicRange was a subset of m_node, rewire into that as well.
				if (m_dynRangeOffset == 0 && m_dynRangeWidth == type.width) {
					in = SignalReadPort(fuseRewire);
				} else {
					auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
					rewire->connectInput(0, rawDriver());
					rewire->connectInput(1, {.node=fuseRewire, .port=0ull});
					rewire->changeOutputType(type);
					rewire->setReplaceRange(m_dynRangeOffset);

					in = SignalReadPort(rewire);
				}
			} else {
				// Static selection

				auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
				rewire->connectInput(0, rawDriver());
				rewire->connectInput(1, in);
				rewire->changeOutputType(type);
				size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
				rewire->setReplaceRange(offset);

				in = SignalReadPort(rewire);
			}
		}

		// In any case, build multiplexer in case the assignment is conditional
		if (auto* scope = ConditionalScope::get(); !ignoreConditions && scope && scope->getId() > m_initialScopeId)
		{
			auto* signal_in = DesignScope::createNode<hlim::Node_Signal>();
			signal_in->connectInput(rawDriver());

			auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
			mux->connectInput(0, {.node = signal_in, .port = 0});
			mux->connectInput(1, in); // assign rhs last in case previous port was undefined
			mux->connectSelector(scope->getFullCondition());
			mux->setConditionId(scope->getId());

			in = SignalReadPort(mux);
		}

		m_node->connectInput(in);
	}

	SignalReadPort Bit::rawDriver() const
	{
		hlim::NodePort driver = m_node->getDriver(0);
		if (!driver.node)
			driver = hlim::NodePort{ .node = m_node, .port = 0ull };
		return SignalReadPort(driver);
	}

	bool Bit::valid() const
	{
		return true;
	}

	Bit pipestage(const Bit& signal)
	{
		auto* pipeStage = DesignScope::createNode<hlim::Node_RegHint>();
		pipeStage->connectInput(signal.readPort());
		return Bit{ SignalReadPort{pipeStage}, signal.resetValue() };
	}

	Bit pipeinput(const Bit& signal, PipeBalanceGroup& group)
	{
		if(signal.resetValue())
		{
			return pipeinput<Bit, bool>(signal, *signal.resetValue(), group);
		}
		else
		{
			return pipeinput<Bit>(signal, group);
		}
	}
}
