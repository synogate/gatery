/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "Comments.h"
#include <hcl/hlim/NodeGroup.h>
#include <hcl/hlim/Circuit.h>
#include <hcl/utils/Preprocessor.h>
#include <hcl/simulation/ConstructionTimeSimulationContext.h>

namespace hcl::core::frontend {
    
    
template<class FinalType>
class BaseScope
{
    class Lock {
    public:
        Lock(Lock&& o) : m_ptr(o.m_ptr) { o.m_ptr = nullptr; }
        Lock(const Lock&) = delete;
        Lock(FinalType* ptr) : m_ptr(ptr) {}
        ~Lock();

        FinalType* operator -> () { return m_ptr; }
        operator bool() const { return m_ptr != nullptr; }
    private:
        FinalType* m_ptr;
    };

    public:
        BaseScope() { m_parentScope = m_currentScope; m_currentScope = static_cast<FinalType*>(this); }
        ~BaseScope() { m_currentScope = m_parentScope; }

        static Lock lock() { Lock ret = m_currentScope; m_currentScope = nullptr; return ret; }
    protected:
        static void unlock(FinalType* scope) { assert(!m_currentScope); m_currentScope = scope; }

        FinalType *m_parentScope;
        static thread_local FinalType *m_currentScope;
};

template<class FinalType>
thread_local FinalType *BaseScope<FinalType>::m_currentScope = nullptr;
    


class GroupScope : public BaseScope<GroupScope>
{
    public:
        using GroupType = hlim::NodeGroup::GroupType;
        
        GroupScope(GroupType groupType);
        GroupScope(hlim::NodeGroup *nodeGroup);
        
        GroupScope &setName(std::string name);
        GroupScope &setComment(std::string comment);
        
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

        static void visualize(const std::string &filename);

        static DesignScope *get() { return m_currentScope; }
        hlim::Circuit &getCircuit() { return m_circuit; }
        
        template<typename NodeType, typename... Args>
        static NodeType *createNode(Args&&... args);        
        
        template<typename ClockType, typename... Args>
        static ClockType *createClock(Args&&... args);        
    protected:
        hlim::Circuit m_circuit;
        GroupScope m_rootScope;

        sim::ConstructionTimeSimulationContext m_simContext;
        
        // design affecting settings and their overrides go here, as well as tweaking settings (e.g. speed vs area parameters)
};

template<typename NodeType, typename... Args>
NodeType *DesignScope::createNode(Args&&... args) {
    HCL_ASSERT(GroupScope::getCurrentNodeGroup() != nullptr);
    
    NodeType *node = m_currentScope->m_circuit.createNode<NodeType>(std::forward<Args>(args)...);
    node->recordStackTrace();
    node->moveToGroup(GroupScope::getCurrentNodeGroup());
    node->setComment(Comments::retrieve());
    return node;
}

template<typename ClockType, typename... Args>
ClockType *DesignScope::createClock(Args&&... args) {
    ClockType *node = m_currentScope->m_circuit.createClock<ClockType>(std::forward<Args>(args)...);
    return node;
}

template<class FinalType>
inline BaseScope<FinalType>::Lock::~Lock()
{
    if(m_ptr)
        BaseScope<FinalType>::unlock(m_ptr);
}

}
