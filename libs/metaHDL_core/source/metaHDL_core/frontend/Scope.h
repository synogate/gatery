#pragma once

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"

namespace mhdl::core::frontend {
    
    
template<class FinalType>
class BaseScope
{
    public:
        BaseScope() { m_parentScope = m_currentScope; m_currentScope = this; }
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
        GroupScope();
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
        
        hlim::Circuit &getCircuit() { return m_circuit; }
    protected:
        hlim::Circuit m_circuit;
        GroupScope m_rootScope;
        
        // design affecting settings and their overrides go here, as well as tweaking settings (e.g. speed vs area parameters)
};

}
