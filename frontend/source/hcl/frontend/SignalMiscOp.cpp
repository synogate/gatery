#include "SignalMiscOp.h"

#include <hcl/utils/Range.h>
#include <hcl/utils/Preprocessor.h>


namespace hcl::core::frontend {


SignalTapHelper::SignalTapHelper(hlim::Node_SignalTap::Level level)
{
    m_node = DesignScope::createNode<hlim::Node_SignalTap>();
    m_node->recordStackTrace();
    m_node->setLevel(level);
}

unsigned SignalTapHelper::addInput(hlim::NodePort nodePort)
{
    for (auto i : utils::Range(m_node->getNumInputPorts()))
        if (m_node->getDriver(i) == nodePort)
            return i;
    m_node->addInput(nodePort);
    return m_node->getNumInputPorts()-1;
}

void SignalTapHelper::triggerIf(const Bit &condition)
{
    HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
    addInput({.node = condition.getNode(), .port = 0ull});
    m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH);
}

void SignalTapHelper::triggerIfNot(const Bit &condition)
{
    HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
    addInput({.node = condition.getNode(), .port = 0ull});
    m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_LOW);
}

    
SignalTapHelper &SignalTapHelper::operator<<(const std::string &msg)
{
    m_node->addMessagePart(msg);
    return *this;
}
    
SignalTapHelper sim_assert(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_ASSERT);
    helper.triggerIfNot(condition);    
    return helper;
}

SignalTapHelper sim_warnIf(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_WARN);
    helper.triggerIf(condition);    
    return helper;
}

    
SignalTapHelper sim_debug()
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
    return helper;
}

SignalTapHelper sim_debugIf(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
    helper.triggerIf(condition);    
    return helper;
}



}
