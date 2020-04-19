#pragma once

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"

namespace mhdl::core::frontend {

class RootScope;
    
class Scope
{
    public:
        Scope();
        ~Scope();
        
        Scope &setName(std::string name);
        
        static Scope *getCurrentScope() { return m_currentScope; }
        static hlim::NodeGroup *getCurrentNodeGroup() { return m_currentScope==nullptr?nullptr:m_currentScope->m_nodeGroup; }
    protected:
        Scope(RootScope*);
        
        Scope *m_parentScope;
        static thread_local Scope *m_currentScope;
        
        hlim::NodeGroup *m_nodeGroup;
};

class RootScope : public Scope
{
    public:
        RootScope();
        
        inline hlim::Circuit &getCircuit() { return m_circuit; }
    protected:
        hlim::Circuit m_circuit;
};

}
