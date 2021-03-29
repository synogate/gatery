#include "Node_Signal.h"

#include "../SignalGroup.h"

#include "../../utils/Exceptions.h"

#include <string>

namespace hcl::core::hlim {

void Node_Signal::setConnectionType(const ConnectionType &connectionType)
{
    setOutputConnectionType(0, connectionType);
}

void Node_Signal::connectInput(const NodePort &nodePort)
{
    if (nodePort.node != nullptr) {
        auto paramType = nodePort.node->getOutputConnectionType(nodePort.port);
        auto myType = getOutputConnectionType(0);
        if (!getDirectlyDriven(0).empty()) {
            HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
        } else
            setConnectionType(paramType);
    }
    NodeIO::connectInput(0, nodePort);
}

void Node_Signal::disconnectInput()
{
    NodeIO::disconnectInput(0);
}

void Node_Signal::moveToSignalGroup(SignalGroup *group)
{
    if (m_signalGroup != nullptr) {
        auto it = std::find(m_signalGroup->m_nodes.begin(), m_signalGroup->m_nodes.end(), this);
        HCL_ASSERT(it != m_signalGroup->m_nodes.end());

        *it = std::move(m_signalGroup->m_nodes.back());
        m_signalGroup->m_nodes.pop_back();
    }

    m_signalGroup = group;
    if (m_signalGroup != nullptr)
        m_signalGroup->m_nodes.push_back(this);
}

std::unique_ptr<BaseNode> Node_Signal::cloneUnconnected() const {
    std::unique_ptr<BaseNode> copy(new Node_Signal());
    copyBaseToClone(copy.get());

    return copy;
}

std::string Node_Signal::attemptInferOutputName(size_t outputPort) const
{
    if (m_name.empty()) return "";

    size_t underscorePos = m_name.find_last_of('_');
    if (underscorePos == std::string::npos)
        return m_name + "_2";

    std::string potentialDigits(m_name.begin()+underscorePos+1, m_name.end());

    unsigned digits;
    try {
        digits = std::stoul(potentialDigits);
    } catch (...) {
        // not a digit
        return m_name + "_2";
    }

    return std::string(m_name.begin(), m_name.begin()+underscorePos) + std::to_string(digits+1);
}


}
