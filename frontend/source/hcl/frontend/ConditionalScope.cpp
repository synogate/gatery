#include "ConditionalScope.h"

#include "Bit.h"
#include "Signal.h"


#include <hcl/hlim/coreNodes/Node_Multiplexer.h>


namespace hcl::core::frontend {

thread_local hlim::NodePort ConditionalScope::m_lastCondition;
thread_local std::set<hlim::Node_Multiplexer*> ConditionalScope::m_handoverLastConditionMultiplexers;

ConditionalScope::ConditionalScope(const Bit &condition)
{
    m_condition = condition.getReadPort();
    m_isElsePart = false;
}

ConditionalScope::ConditionalScope()
{
    m_lastConditionMultiplexers = m_handoverLastConditionMultiplexers;
    m_condition = m_lastCondition;
    m_isElsePart = true;
}

ConditionalScope::~ConditionalScope()
{
    buildConditionalMuxes();
    m_lastCondition = m_condition;
    if (!m_isElsePart)
        m_handoverLastConditionMultiplexers = m_lastConditionMultiplexers;
}

void ConditionalScope::registerConditionalAssignment(ElementarySignal *signal, hlim::NodePort previousOutput)
{
    hlim::NodePort conditionalOutput = signal->getReadPort();
    
    // There is a problem here with assigning unconnected signals condtionally. All unconnected signals land in the same "bucket". I need to think about this more.
    HCL_ASSERT(!m_conditional2previousOutput.contains(conditionalOutput));
    
    
    // Check if the previous assignment to this signal was also conditioned on the same (this) condition. If so, directly "forward" to an earlier assignment that was not conditional.
    auto it = m_conditional2previousOutput.find(previousOutput);
    if (it != m_conditional2previousOutput.end())
        previousOutput = it->second;
    
    m_conditional2previousOutput[conditionalOutput] = previousOutput;
    m_conditionalyAssignedSignals.insert(signal);
}

void ConditionalScope::unregisterSignal(ElementarySignal *signal)
{
    m_conditionalyAssignedSignals.erase(signal);
}


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



}
