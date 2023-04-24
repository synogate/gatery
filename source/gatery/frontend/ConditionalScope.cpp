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
#include "ConditionalScope.h"
#include "DesignScope.h"

#include "Signal.h"
#include "Bit.h"

#include "SignalLogicOp.h"

#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>


namespace gtry {

	thread_local hlim::NodePort ConditionalScope::m_lastCondition;
	thread_local size_t ConditionalScope::s_nextId = 1;

	thread_local static std::optional<Bit> g_lastConditionBit;

#define SPAM_SIGNAL_NODES
	
ConditionalScope::ConditionalScope(const Bit &condition, bool override) :
	m_id(s_nextId++)
{
	setCondition(condition.readPort(), override);
	m_isElseScope = false;
}

ConditionalScope::ConditionalScope() :
	m_id(s_nextId++)
{
	m_isElseScope = true;
	hlim::Node_Logic* invNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::NOT);
	invNode->connectInput(0, m_lastCondition);

#ifdef SPAM_SIGNAL_NODES
	hlim::Node_Signal* sigNode = DesignScope::createNode<hlim::Node_Signal>();
	sigNode->connectInput({ .node = invNode, .port = 0ull });
	if (!m_lastCondition.node->getName().empty())
		sigNode->setName(std::string("not_")+m_lastCondition.node->getName());
	setCondition({ .node = sigNode, .port = 0ull });
#else
	setCondition({ .node = invNode, .port = 0ull });
#endif
	
}

ConditionalScope::~ConditionalScope()
{
	if (!m_isElseScope) {
		m_lastCondition = m_condition;
	} else {
		hlim::Node_Logic* invNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::NOT);
		invNode->connectInput(0, m_condition);
		hlim::Node_Logic* andNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::OR);
		andNode->connectInput(0, m_lastCondition);
		andNode->connectInput(1, {.node=invNode, .port=0u});

		m_lastCondition = {.node=andNode, .port=0u};
	}
	g_lastConditionBit.reset();
}
/*
const Bit& ConditionalScope::getCurrentCondition()
{
	if (!g_lastConditionBit) {
		if (m_lastCondition.node == nullptr)
			g_lastConditionBit.emplace('1');
		else
			g_lastConditionBit.emplace(SignalReadPort(m_lastCondition));
	}
	return *g_lastConditionBit;
}
*/
void ConditionalScope::setCondition(hlim::NodePort port, bool override)
{
	m_condition = port;
	m_fullCondition = port;

	if (m_parentScope && !override)
	{
		hlim::Node_Logic* andNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::AND);
		andNode->connectInput(0, m_condition);
		andNode->connectInput(1, m_parentScope->m_fullCondition);
		
#ifdef SPAM_SIGNAL_NODES
		hlim::Node_Signal* sigNode = DesignScope::createNode<hlim::Node_Signal>();
		sigNode->connectInput({ .node = andNode, .port = 0ull });
		if (!port.node->getName().empty())
			sigNode->setName(std::string("nested_condition_")+port.node->getName());
		m_fullCondition = { .node = sigNode, .port = 0ull };
#else
		m_fullCondition = { .node = andNode, .port = 0ull };
#endif
	}
	m_enScope.setup(m_fullCondition);
}

Bit ConditionalScope::globalEnable()
{
	if (m_currentScope != nullptr)
		return Bit(SignalReadPort(m_currentScope->getFullCondition()));
	else
		return '1';
}


}
