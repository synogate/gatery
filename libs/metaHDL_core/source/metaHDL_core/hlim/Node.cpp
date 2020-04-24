#include "Node.h"

#include "NodeGroup.h"

#include "../utils/Exceptions.h"
#include "../utils/Range.h"

namespace mhdl::core::hlim {

BaseNode::BaseNode() 
{
}

BaseNode::BaseNode(size_t numInputs, size_t numOutputs)
{
    resizeInputs(numInputs);
    resizeOutputs(numOutputs);
}

BaseNode::~BaseNode() 
{ 
    moveToGroup(nullptr); 
}


bool BaseNode::isOrphaned() const
{
    for (auto i : utils::Range(getNumInputPorts()))
        if (getDriver(i).node != nullptr) return false;
        
    for (auto i : utils::Range(getNumOutputPorts()))
        if (!getDirectlyDriven(i).empty()) return false;
        
    return true;
}

void BaseNode::moveToGroup(NodeGroup *group)
{
    if (group == m_nodeGroup) return;
    
    if (m_nodeGroup != nullptr) {
        auto it = std::find(m_nodeGroup->m_nodes.begin(), m_nodeGroup->m_nodes.end(), this);
        MHDL_ASSERT(it != m_nodeGroup->m_nodes.end());

        *it = m_nodeGroup->m_nodes.back();
        m_nodeGroup->m_nodes.pop_back();
    }
    
    m_nodeGroup = group;
    if (m_nodeGroup != nullptr)
        m_nodeGroup->m_nodes.push_back(this);
}


}
