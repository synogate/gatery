#include "SignalGroup.h"

#include "coreNodes/Node_Signal.h"

namespace hcl::core::hlim {

    
SignalGroup::SignalGroup(GroupType groupType) : m_groupType(groupType)
{
}
    
SignalGroup::~SignalGroup()
{
    while (!m_nodes.empty())
        m_nodes.front()->moveToSignalGroup(nullptr);
}


SignalGroup *SignalGroup::addChildSignalGroup(GroupType groupType)
{
    m_children.push_back(std::make_unique<SignalGroup>(groupType));
    m_children.back()->m_parent = this;
    return m_children.back().get();
}



bool SignalGroup::isChildOf(const SignalGroup *other) const
{
    const SignalGroup *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}



}
