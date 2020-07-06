#include "ConditionalScope.h"

#include "Bit.h"
#include "Signal.h"


#include <hcl/hlim/coreNodes/Node_Multiplexer.h>


namespace hcl::core::frontend {

thread_local hlim::NodePort ConditionalScope::m_lastCondition;

ConditionalScope::ConditionalScope(const Bit &condition)
{
    m_condition = {.node = condition.getNode(), .port = 0ull};
    m_negateCondition = false;
}

ConditionalScope::ConditionalScope()
{
    m_condition = m_lastCondition;
    m_negateCondition = true;
}

ConditionalScope::~ConditionalScope()
{
    buildConditionalMuxes();
    m_lastCondition = m_condition;
}

void ConditionalScope::registerConditionalAssignment(ElementarySignal *signal, hlim::NodePort previousOutput)
{
    hlim::NodePort conditionalOutput = {.node = signal->getNode(), .port = 0ull};
    
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
        hlim::NodePort conditionalOutput = {.node = signal->getNode(), .port = 0ull};
        auto it = m_conditional2previousOutput.find(conditionalOutput);
        HCL_ASSERT(it != m_conditional2previousOutput.end());
        hlim::NodePort previousOutput = it->second;


        hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(2);
        node->recordStackTrace();
        node->connectSelector(m_condition);
        if (m_negateCondition) {
            node->connectInput(0, conditionalOutput);
            node->connectInput(1, previousOutput);
        } else {
            node->connectInput(1, conditionalOutput);
            node->connectInput(0, previousOutput);
        }
        
        signal->assignConditionalScopeMuxOutput({.node = node, .port = 0ull}, nullptr);
        if (m_parentScope != nullptr) {
            m_parentScope->registerConditionalAssignment(signal, previousOutput);
        }
    }
}



}
