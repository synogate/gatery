#pragma once

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"
#include "../utils/Preprocessor.h"

namespace mhdl::core::frontend {
    
    
template<class FinalType>
class BaseScope
{
    public:
        BaseScope() { m_parentScope = m_currentScope; m_currentScope = static_cast<FinalType*>(this); }
        ~BaseScope() { m_currentScope = m_parentScope; }
    protected:
        FinalType *m_parentScope;
        static thread_local FinalType *m_currentScope;
};

template<class FinalType>
thread_local FinalType *BaseScope<FinalType>::m_currentScope = nullptr;
    


class GroupScope : public BaseScope<GroupScope>
{
    public:
        GroupScope(hlim::NodeGroup::GroupType groupType);
        GroupScope(hlim::NodeGroup *nodeGroup);
        
        GroupScope &setName(std::string name);
        
        static GroupScope *get() { return m_currentScope; }
        static hlim::NodeGroup *getCurrentNodeGroup() { return m_currentScope==nullptr?nullptr:m_currentScope->m_nodeGroup; }
    protected:
        hlim::NodeGroup *m_nodeGroup;
};

class FactoryOverride : public BaseScope<FactoryOverride>
{
    public:
        static FactoryOverride *get() { return m_currentScope; }
    protected:
};


class DesignScope : public BaseScope<DesignScope>
{
    public:
        DesignScope();

        static DesignScope *get() { return m_currentScope; }
        hlim::Circuit &getCircuit() { return m_circuit; }
        
        template<typename NodeType, typename... Args>
        static NodeType *createNode(Args&&... args);        
        
    protected:
        hlim::Circuit m_circuit;
        GroupScope m_rootScope;
        
        // design affecting settings and their overrides go here, as well as tweaking settings (e.g. speed vs area parameters)
};

template<typename NodeType, typename... Args>
NodeType *DesignScope::createNode(Args&&... args) {
    MHDL_ASSERT(GroupScope::getCurrentNodeGroup() != nullptr);
    
    MHDL_DESIGNCHECK_HINT(GroupScope::getCurrentNodeGroup()->getGroupType() == hlim::NodeGroup::GRP_AREA ||
                          GroupScope::getCurrentNodeGroup()->getGroupType() == hlim::NodeGroup::GRP_PROCEDURE, "Nodes creation can only happen inside GROUP_AREA or GROUP_PROCEDURE scopes!");
    
    NodeType *node = m_currentScope->m_circuit.createNode<NodeType>(std::forward<Args>(args)...);
    node->moveToGroup(GroupScope::getCurrentNodeGroup());
    return node;
}


}
