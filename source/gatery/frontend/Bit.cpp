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

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/supportNodes/Node_Default.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/supportNodes/Node_Attributes.h>
#include <gatery/hlim/supportNodes/Node_RegHint.h>
#include <gatery/hlim/supportNodes/Node_RetimingBlocker.h>


namespace gtry {

	BitDefault::BitDefault(const Bit& rhs)
	{
		m_nodePort = rhs.readPort();
	}

	void BitDefault::assign(bool value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBit(value), hlim::ConnectionType::BOOL
			);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	void BitDefault::assign(char value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBit(value), hlim::ConnectionType::BOOL
			);
		m_nodePort = {.node = constant, .port = 0ull };
	}

	Bit reg(const Bit& val, const RegisterSettings& settings)
	{
		if (auto rval = val.resetValue())
		{
			Bit resetVal{ *rval };
			return Bit{
				internal::reg(val.readPort(), resetVal.readPort(), settings),
				rval
			};
		}

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

	Bit::Bit(hlim::Node_Signal* node, const std::shared_ptr<BitVectorSlice>& slice, size_t initialScopeId) :
		m_node(node),
		m_slice(slice)
	{
		m_slice->makeItABit();
		m_initialScopeId = initialScopeId;
	}

	Bit& Bit::operator=(Bit&& rhs)
	{
		auto port = rhs.node()->getDriver(0);
		if (port.node == nullptr)
		{
			// special case where we move a unassigned signal into an existing signal
			// an implementation with conditional scopes is possibile but has many corner cases
			// think of assigning a signal that is only conditionally loopy
			HCL_ASSERT_HINT(ConditionalScope::get() == nullptr, "no impl");
			HCL_ASSERT_HINT(hlim::getOutputConnectionType(readPort()).type == hlim::ConnectionType::BOOL, "cannot move loops into vector aliases");

			m_node = hlim::NodePtr<hlim::Node_Signal>{};
			createNode();

			m_resetValue.reset();
			m_resetValue = rhs.resetValue();
		}
		else
		{
			if (rhs.m_resetValue)
				m_resetValue = rhs.m_resetValue;

			assign(rhs.readPort());
		}
	
		rhs.assign(outPort());
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

	void Bit::simulationOverride(const Bit& simulationOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(simulationOverride.readPort());
		expOverride->connectOverride(readPort());
		assign(SignalReadPort(expOverride));
	}

	BitWidth Bit::width() const
	{
		return 1_b;
	}

	hlim::ConnectionType Bit::connType() const
	{
		return hlim::ConnectionType{
			.type = hlim::ConnectionType::BOOL,
			.width = 1
		};
	}

	SignalReadPort Bit::readPort() const
	{
		SignalReadPort port = rawDriver();
		if (m_slice)
			port = m_slice->readPort(port);
		return port;
	}

	SignalReadPort Bit::outPort() const
	{
		SignalReadPort port{ m_node };
		if (m_slice)
			port = m_slice->readPort(port);
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

	void Bit::setName(std::string name) const
	{
		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();
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

	void Bit::resetValueRemove()
	{
		m_resetValue.reset();
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
			sim::parseBit(value), hlim::ConnectionType::BOOL
			);
		assign(SignalReadPort(constant));
	}

	void Bit::assign(char value)
	{
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBit(value), hlim::ConnectionType::BOOL
			);
		assign(SignalReadPort(constant));
	}

	void Bit::assign(SignalReadPort in, bool ignoreConditions)
	{
		if (m_slice)
			in = m_slice->assign(rawDriver(), in);

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
			Bit res = pipeinput<Bit, bool>(signal, *signal.resetValue(), group);
			res.resetValue(*signal.resetValue());
			return res;
		}
		else
		{
			return pipeinput<Bit>(signal, group);
		}
	}

	Bit retimingBlocker(const Bit& signal)
	{
		auto* node = DesignScope::createNode<hlim::Node_RetimingBlocker>();
		node->connectInput(signal.readPort());
		return Bit{ SignalReadPort{node}, signal.resetValue() };
	}
	
	Bit final(const Bit& signal)
	{
		return Bit{ signal.outPort(), signal.resetValue() };
	}

	BVec Bit::toBVec() const 
	{ 
		BVec b = 1_b; 
		b[0] = *this; 
		return b; 
	}
	
	void Bit::fromBVec(const BVec &bvec)
	{ 
		(*this) = bvec.lsb(); 
	}

}
