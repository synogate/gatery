#include "Scope.h"

#include <stdexcept>

namespace mhdl::core::frontend {

thread_local Scope *Scope::m_currentScope = nullptr;

Scope::Scope()
{
    m_parentScope = m_currentScope;
    m_currentScope = this;
    
    m_nodeGroup = m_parentScope->m_nodeGroup->addChildNodeGroup();
    m_nodeGroup->recordStackTrace();
}

Scope::Scope(RootScope*)
{
    if (m_currentScope != nullptr)
        throw std::runtime_error("Only one root scope can exist!");
    m_parentScope = nullptr;
    m_currentScope = this;
}

Scope::~Scope()
{
    m_currentScope = m_parentScope;
}

Scope &Scope::setName(std::string name)
{
    m_nodeGroup->setName(std::move(name));
    return *this;
}


RootScope::RootScope() : Scope(this) 
{
    m_nodeGroup = m_circuit.getRootNodeGroup();
    m_nodeGroup->recordStackTrace();
}


}
