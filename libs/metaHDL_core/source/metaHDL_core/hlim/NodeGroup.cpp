#include "NodeGroup.h"

#include "coreNodes/Node_Signal.h"

namespace mhdl::core::hlim {

NodeGroup *NodeGroup::addChildNodeGroup()
{
    m_children.push_back(std::make_unique<NodeGroup>());
    m_children.back()->m_parent = this;
    return m_children.back().get();
}



bool NodeGroup::isChildOf(const NodeGroup *other) const
{
    const NodeGroup *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}



}
