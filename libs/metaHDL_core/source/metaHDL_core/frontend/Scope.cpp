#include "Scope.h"


namespace mhdl::core::frontend {
    
GroupScope::GroupScope() : BaseScope<GroupScope>()
{
    m_nodeGroup = m_parentScope->m_nodeGroup->addChildNodeGroup();
    m_nodeGroup->recordStackTrace();
}

GroupScope::GroupScope(hlim::NodeGroup *nodeGroup)
{
    m_nodeGroup = nodeGroup;
}


GroupScope &GroupScope::setName(std::string name)
{
    m_nodeGroup->setName(std::move(name));
    return *this;
}


DesignScope::DesignScope() : BaseScope<DesignScope>(), m_rootScope(m_circuit.getRootNodeGroup())
{ 
    m_rootScope.setName("root");
    
    MHDL_DESIGNCHECK_HINT(m_parentScope == nullptr, "Only one design scope can be active at a time!");
}

}
