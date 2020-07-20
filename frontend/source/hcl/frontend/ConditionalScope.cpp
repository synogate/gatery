#include "ConditionalScope.h"

#include "Signal.h"
#include "Bit.h"

#include "SignalLogicOp.h"

#include <hcl/hlim/coreNodes/Node_Multiplexer.h>


namespace hcl::core::frontend {

    thread_local hlim::NodePort ConditionalScope::m_lastCondition;

    thread_local static std::optional<Bit> g_lastConditionBit;

ConditionalScope::ConditionalScope(const Bit &condition)
{
    setCondition(condition.getReadPort());
}

ConditionalScope::ConditionalScope()
{
    hlim::Node_Logic* invNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::NOT);
    invNode->connectInput(0, m_lastCondition);
    setCondition({ .node = invNode, .port = 0ull });
}

ConditionalScope::~ConditionalScope()
{
    m_lastCondition = m_condition;
    g_lastConditionBit.reset();
}

const Bit& ConditionalScope::getCurrentCondition()
{
    if (!g_lastConditionBit)
        g_lastConditionBit.emplace(m_lastCondition);
    return *g_lastConditionBit;
}

void ConditionalScope::setCondition(hlim::NodePort port)
{
    m_condition = port;
    m_fullCondition = port;

    if (m_parentScope)
    {
        hlim::Node_Logic* andNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::AND);
        andNode->connectInput(0, m_condition);
        andNode->connectInput(1, m_parentScope->m_fullCondition);
        m_fullCondition = { .node = andNode, .port = 0ull };
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
