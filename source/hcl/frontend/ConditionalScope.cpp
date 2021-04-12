/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "ConditionalScope.h"

#include "Signal.h"
#include "Bit.h"

#include "SignalLogicOp.h"

#include <hcl/hlim/coreNodes/Node_Multiplexer.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>


namespace hcl::core::frontend {

    thread_local hlim::NodePort ConditionalScope::m_lastCondition;
    thread_local size_t ConditionalScope::s_nextId = 1;

    thread_local static std::optional<Bit> g_lastConditionBit;

#define SPAM_SIGNAL_NODES
    
ConditionalScope::ConditionalScope(const Bit &condition) :
    m_id(s_nextId++)
{
    setCondition(condition.getReadPort());
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
void ConditionalScope::setCondition(hlim::NodePort port)
{
    m_condition = port;
    m_fullCondition = port;

    if (m_parentScope)
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
}

#if 0
void ConditionalScope::buildConditionalMuxes()
{
    for (auto signal : m_conditionalyAssignedSignals) {
        hlim::NodePort conditionalOutput = signal->getReadPort();
        auto it = m_conditional2previousOutput.find(conditionalOutput);
        HCL_ASSERT(it != m_conditional2previousOutput.end());
        hlim::NodePort previousOutput = it->second;

        hlim::Node_Multiplexer *previousMultiplexerNode = dynamic_cast<hlim::Node_Multiplexer *>(previousOutput.node);
        if (previousMultiplexerNode == nullptr) {
            hlim::Node_Signal *previousSignalNode = dynamic_cast<hlim::Node_Signal *>(previousOutput.node);
            if (previousSignalNode != nullptr)
                previousMultiplexerNode = dynamic_cast<hlim::Node_Multiplexer*>(previousSignalNode->getNonSignalDriver(0).node);
        }
        //
        
        /*
         * This is broken, so it's disabled for now...
         * It breaks in a construction such as this:
         * IF (condition1) {
         *      c = ...;
         * } ELSE {
         *      IF (condition2)
         *          c = ...;
         * }
         * The cascaded multiplexer is in the ELSE part and thus gets attached to the input of the outer multiplexer. However, he cascaded 
         * multiplexer uses the modified c value (modified from the outer IF branch) as input, thus building a cyclic dependency.
         */
        if (false && m_isElsePart && previousMultiplexerNode != nullptr && m_lastConditionMultiplexers.contains(previousMultiplexerNode)) {
            // We are in the ELSE part, and this signal was also assigned in the IF part.
            // It already has a multiplexer from the IF part, so we replace the corresponding input of that multiplexer instead of chaining another one.
            previousMultiplexerNode->connectInput(0, conditionalOutput);

            hlim::NodePort output{.node = previousMultiplexerNode, .port = 0ull};
            signal->assignConditionalScopeMuxOutput(output, m_parentScope);
            if (m_parentScope != nullptr) {
                m_parentScope->registerConditionalAssignment(signal, previousOutput);
            }
        } else {

            hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            node->recordStackTrace();
            node->connectSelector(m_condition);
        
            // always assign conditionalOutput last to derive output connection type from that one.
            if (m_isElsePart) {
                node->connectInput(1, previousOutput);
                node->connectInput(0, conditionalOutput);
            } else {
                node->connectInput(0, previousOutput);
                node->connectInput(1, conditionalOutput);
            }
            
            hlim::NodePort output{.node = node, .port = 0ull};
            
            if (!m_isElsePart)
                m_lastConditionMultiplexers.insert(node);
            
            signal->assignConditionalScopeMuxOutput(output, m_parentScope);
            if (m_parentScope != nullptr) {
                m_parentScope->registerConditionalAssignment(signal, previousOutput);
            }
        }
    }
}
#endif


}
