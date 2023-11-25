/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "Enum.h"

#include "UInt.h"
#include "Reg.h"

#include "DesignScope.h"
#include "ConditionalScope.h"

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/coreNodes/Node_Compare.h>
#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
//#include <gatery/hlim/Attributes.h>


namespace gtry::internal_enum {
	SignalReadPort makeCompareNode(hlim::Node_Compare::Op op, NormalizedWidthOperands ops)
	{
		auto* node = DesignScope::createNode<hlim::Node_Compare>(op);
		node->recordStackTrace();
		node->connectInput(0, ops.lhs);
		node->connectInput(1, ops.rhs);

		return SignalReadPort(node);
	}

	SignalReadPort makeCompareNodeEQ(NormalizedWidthOperands ops)
	{
		return makeCompareNode(hlim::Node_Compare::EQ, ops);
	}

	SignalReadPort makeCompareNodeNEQ(NormalizedWidthOperands ops)
	{
		return makeCompareNode(hlim::Node_Compare::NEQ, ops);
	}

	SignalReadPort reg(SignalReadPort val, std::optional<SignalReadPort> reset, const RegisterSettings& settings) {
		return internal::reg(val, reset, settings);
	}
}


namespace gtry
{
	void BaseEnum::exportOverride(const SignalReadPort& exportOverride) {
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(readPort());
		expOverride->connectOverride(exportOverride);
		assign(SignalReadPort(expOverride));		
	}

	void BaseEnum::simulationOverride(const SignalReadPort& simulationOverride)
	{
		auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
		expOverride->connectInput(simulationOverride);
		expOverride->connectOverride(readPort());
		assign(SignalReadPort(expOverride));
	}

	hlim::ConnectionType BaseEnum::connType() const {
		return hlim::ConnectionType{
			.type = hlim::ConnectionType::BITVEC,
			.width = width().value
		};
	}

	SignalReadPort BaseEnum::outPort() const { 
		return SignalReadPort{ m_node }; 
	}

	std::string_view BaseEnum::getName() const {
		if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
			return sigNode->getName();
		return {};
	}

	void BaseEnum::setName(std::string name) {
		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();

		assign(SignalReadPort(signal), true);
	}

	void BaseEnum::setName(std::string name) const
	{
		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput(readPort());
		signal->setName(name);
		signal->recordStackTrace();
	}

	void BaseEnum::addToSignalGroup(hlim::SignalGroup *signalGroup)  {
		m_node->moveToSignalGroup(signalGroup);
	}

	void BaseEnum::createNode() {
		HCL_ASSERT(!m_node);
		m_node = DesignScope::createNode<hlim::Node_Signal>();
		m_node->setConnectionType(connType());
		m_node->recordStackTrace();
	}	

	void BaseEnum::assign(size_t v, std::string_view name) {
		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			sim::parseBitVector(v, width().value), hlim::ConnectionType::BITVEC
		);
		constant->setName(std::string(name));
		assign(SignalReadPort(constant));
	}

	void BaseEnum::assign(SignalReadPort in, bool ignoreConditions) {
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
	
	
	SignalReadPort BaseEnum::rawDriver() const {
		hlim::NodePort driver = m_node->getDriver(0);
		if (!driver.node)
			driver = hlim::NodePort{ .node = m_node, .port = 0ull };
		return SignalReadPort(driver);
	}

	void BaseEnum::connectInput(const SignalReadPort& port) {
		m_node->connectInput(port);
	}

}
