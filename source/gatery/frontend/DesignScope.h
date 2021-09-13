/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include <gatery/hlim/NodeGroup.h>
#include <gatery/hlim/Circuit.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/simulation/ConstructionTimeSimulationContext.h>

#include "Scope.h"
#include "tech/targetTechnology.h"

namespace gtry {

class DesignScope : public BaseScope<DesignScope>
{
    public:
        DesignScope();
        DesignScope(std::unique_ptr<TargetTechnology> targetTech);

        static void visualize(const std::string &filename, hlim::NodeGroup *nodeGroup = nullptr);

        static DesignScope *get() { return m_currentScope; }
        hlim::Circuit &getCircuit() { return m_circuit; }
        
        template<typename NodeType, typename... Args>
        static NodeType *createNode(Args&&... args);        
        
        template<typename ClockType, typename... Args>
        static ClockType *createClock(Args&&... args);

        inline GroupScope &getRootGroup() { return m_rootScope; }

        
    protected:
        hlim::Circuit m_circuit;
        GroupScope m_rootScope;

        sim::ConstructionTimeSimulationContext m_simContext;
        
        std::unique_ptr<TargetTechnology> m_targetTech;
        TechnologyScope m_defaultTechScope;
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

}
