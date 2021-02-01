#include "NodeGroup.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"

namespace hcl::core::hlim {

    
NodeGroup::NodeGroup(GroupType groupType) : m_groupType(groupType)
{
}
    
NodeGroup::~NodeGroup()
{
    while (!m_nodes.empty())
        m_nodes.front()->moveToGroup(nullptr);
}


NodeGroup *NodeGroup::addChildNodeGroup(GroupType groupType)
{
    m_children.push_back(std::make_unique<NodeGroup>(groupType));
    m_children.back()->m_parent = this;
    return m_children.back().get();
}

void NodeGroup::moveInto(NodeGroup *newParent)
{
    size_t parentIdx = ~0ull;
    for (auto i : utils::Range(m_parent->m_children.size()))
        if (m_parent->m_children[i].get() == this) {
            parentIdx = i;
            break;
        }

    newParent->m_children.push_back(std::move(m_parent->m_children[parentIdx]));
    m_parent->m_children[parentIdx] = std::move(m_parent->m_children.back());
    m_parent->m_children.pop_back();
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
