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

#include "ConstructionHelper.h"

#include "Circuit.h"
#include "NodeGroup.h"
#include "Subnet.h"
#include "Clock.h"
#include "coreNodes/Node_Signal.h"
#include "supportNodes/Node_Attributes.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Arithmetic.h"
#include "coreNodes/Node_Compare.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_ClkRst2Signal.h"
#include "../simulation/BitVectorState.h"

namespace gtry::hlim {

	ConstructionHelper::ConstructionHelper(Circuit &circuit) : m_circuit(circuit)
	{
	}

	ConstructionHelper &ConstructionHelper::constructInGroup(NodeGroup &nodeGroup)
	{
		m_nodeGroup = &nodeGroup;
		return *this;
	}

	ConstructionHelper &ConstructionHelper::newNodesToSubnet(Subnet &subnet)
	{
		m_subnet = &subnet;
		return *this;
	}

	Node_Constant *ConstructionHelper::constBoolean(bool b)
	{	
		sim::DefaultBitVectorState state;
		state.resize(1);
		state.setRange(sim::DefaultConfig::DEFINED, 0, 1);
		if (b)
			state.setRange(sim::DefaultConfig::VALUE, 0, 1);
		else
			state.clearRange(sim::DefaultConfig::VALUE, 0, 1);

		auto *res = m_circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
		newNode(res);
		return res;
	}

	Node_Constant *ConstructionHelper::constBVec(std::uint64_t value, size_t width)
	{
		sim::DefaultBitVectorState state;
		state.resize(width);
		state.setRange(sim::DefaultConfig::DEFINED, 0, width);
		state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, width, value);

		auto *res = m_circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
		newNode(res);
		return res;
	}

	Node_Register *ConstructionHelper::reg(Clock &clock, const NodePort& data)
	{
		auto *node = m_circuit.createNode<Node_Register>();
		newNode(node);
		node->setClock(&clock);
		node->connectInput(Node_Register::DATA, data);
		return node;
	}

	Node_Register *ConstructionHelper::reg(Clock &clock, const NodePort& data, const NodePort& resetValue)
	{
		auto *node = m_circuit.createNode<Node_Register>();
		newNode(node);
		node->setClock(&clock);
		node->connectInput(Node_Register::DATA, data);
		node->connectInput(Node_Register::RESET_VALUE, resetValue);
		return node;
	}

	Node_Multiplexer *ConstructionHelper::mux(const NodePort& sel, const NodePort& sel0, const NodePort& sel1)
	{
		auto *muxNode = m_circuit.createNode<Node_Multiplexer>(2);
		newNode(muxNode);
		muxNode->connectSelector(sel);
		muxNode->connectInput(0, sel0);
		muxNode->connectInput(1, sel1);
		return muxNode;
	}

	Node_Compare *ConstructionHelper::ceq(const NodePort &lhs, const NodePort &rhs)
	{
		auto *compNode = m_circuit.createNode<Node_Compare>(Node_Compare::EQ);
		newNode(compNode);
		compNode->connectInput(0, lhs);
		compNode->connectInput(1, rhs);
		return compNode;	
	}

	Node_Compare *ConstructionHelper::cneq(const NodePort &lhs, const NodePort &rhs)
	{
		auto *compNode = m_circuit.createNode<Node_Compare>(Node_Compare::NEQ);
		newNode(compNode);
		compNode->connectInput(0, lhs);
		compNode->connectInput(1, rhs);
		return compNode;	
	}



	Node_Logic *ConstructionHelper::land(const NodePort &lhs, const NodePort &rhs)
	{
		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		newNode(logicAnd);
		logicAnd->connectInput(0, lhs);
		logicAnd->connectInput(1, rhs);
		return logicAnd;
	}

	Node_Logic *ConstructionHelper::lor(const NodePort &lhs, const NodePort &rhs)
	{
		auto *logicOr = m_circuit.createNode<Node_Logic>(Node_Logic::OR);
		newNode(logicOr);
		logicOr->connectInput(0, lhs);
		logicOr->connectInput(1, rhs);
		return logicOr;
	}

	Node_Logic *ConstructionHelper::lnot(const NodePort &src)
	{
		auto *logicNot = m_circuit.createNode<Node_Logic>(Node_Logic::NOT);
		newNode(logicNot);
		logicNot->connectInput(0, src);
		return logicNot;
	}


	Node_Arithmetic *ConstructionHelper::aadd(const NodePort &lhs, const NodePort &rhs)
	{
		auto *addNode = m_circuit.createNode<Node_Arithmetic>(Node_Arithmetic::ADD);
		newNode(addNode);
		addNode->connectInput(0, lhs);
		addNode->connectInput(1, rhs);
		return addNode;
	}

	Node_Arithmetic *ConstructionHelper::asub(const NodePort &lhs, const NodePort &rhs)
	{
		auto *subNode = m_circuit.createNode<Node_Arithmetic>(Node_Arithmetic::SUB);
		newNode(subNode);
		subNode->connectInput(0, lhs);
		subNode->connectInput(1, rhs);
		return subNode;
	}




	Node_Signal *ConstructionHelper::appendSignal(NodePort &nodePort)
	{
		Node_Signal *sig = m_circuit.createNode<Node_Signal>();
		newNode(sig, nodePort.node != nullptr ? nodePort.node->getGroup() : nullptr);
		if (nodePort.node != nullptr)
			sig->connectInput(nodePort);

		nodePort = p0(sig);
		return sig;
	}

	Node_Signal *ConstructionHelper::appendSignal(RefCtdNodePort &nodePort)
	{
		Node_Signal *sig = m_circuit.createNode<Node_Signal>();
		newNode(sig, nodePort.node != nullptr ? nodePort.node->getGroup() : nullptr);
		if (nodePort.node != nullptr)
			sig->connectInput(nodePort);

		nodePort = p0(sig);
		return sig;
	}

	NodePort ConstructionHelper::isInReset(Clock &clock)
	{
		if (clock.getRegAttribs().resetType == RegisterAttributes::ResetType::NONE) {
			return p0(constBoolean(false));
		} else {
			auto *resetPin = m_circuit.createNode<Node_ClkRst2Signal>();
			resetPin->setClock(&clock);
			newNode(resetPin);

			NodePort res = p0(resetPin);

			if (clock.getRegAttribs().resetActive == RegisterAttributes::Active::LOW)
				res = p0(lnot(res));

			return res;
		}
	}

	Node_Attributes *ConstructionHelper::getCreateAttribNode(NodePort &nodePort)
	{
		if (auto *attrib = dynamic_cast<Node_Attributes*>(nodePort.node))
			return attrib;

		
		auto *attribNode = m_circuit.createNode<Node_Attributes>();
		attribNode->connectInput(nodePort);
		newNode(attribNode, nodePort.node->getGroup());

		nodePort = p0(attribNode);

		return attribNode;
	}



	void ConstructionHelper::newNode(BaseNode *node, NodeGroup *defaultNodeGroup)
	{
		node->recordStackTrace();
		if (m_nodeGroup != nullptr)
			node->moveToGroup(m_nodeGroup);
		else 
			if (defaultNodeGroup != nullptr)
				node->moveToGroup(defaultNodeGroup);

		if (m_subnet) 
			m_subnet->add(node);
	}
}