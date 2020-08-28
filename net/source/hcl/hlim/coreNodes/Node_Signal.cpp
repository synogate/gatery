#include "Node_Signal.h"

#include "../SignalGroup.h"

#include "../../utils/Exceptions.h"

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

void Node_Signal::moveToSignalGroup(SignalGroup *group, unsigned index)
{
    if (m_signalGroup != nullptr) {
        auto it = std::find(m_signalGroup->m_nodes.begin(), m_signalGroup->m_nodes.end(), this);
        HCL_ASSERT(it != m_signalGroup->m_nodes.end());

        *it = nullptr;
    }
    
    m_signalGroup = group;
    if (m_signalGroup != nullptr) {
        if (index == ~0u)
            m_signalGroup->m_nodes.push_back(this);
        else {
            if (index >= m_signalGroup->m_nodes.size())
                m_signalGroup->m_nodes.resize(index+1, nullptr);
            
            if (m_signalGroup->m_nodes[index] != nullptr)
                m_signalGroup->m_nodes[index]->moveToSignalGroup(nullptr);
            m_signalGroup->m_nodes[index] = this;
        }
    }
}



}
