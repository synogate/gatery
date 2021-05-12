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
#include "gatery/pch.h"
#include "BaseGrouping.h"

#include "AST.h"

#include "../../hlim/coreNodes/Node_Signal.h"

namespace gtry::vhdl {

BaseGrouping::BaseGrouping(AST &ast, BaseGrouping *parent, NamespaceScope *parentNamespace) : 
                    m_ast(ast), m_namespaceScope(ast, parentNamespace), m_parent(parent)
{
    
}

BaseGrouping::~BaseGrouping() 
{ 
    
}

bool BaseGrouping::isChildOf(const BaseGrouping *other) const
{
    const BaseGrouping *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}

bool BaseGrouping::isProducedExternally(hlim::NodePort nodePort)
{
    auto driverNodeGroup = m_ast.getMapping().getScope(nodePort.node);
    return driverNodeGroup == nullptr || (driverNodeGroup != this && !driverNodeGroup->isChildOf(this));        
}

bool BaseGrouping::isConsumedExternally(hlim::NodePort nodePort)
{
    for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port)) {
        auto drivenNodeGroup = m_ast.getMapping().getScope(driven.node);
        if (drivenNodeGroup == nullptr || (drivenNodeGroup != this && !drivenNodeGroup->isChildOf(this))) 
            return true;
    }

    return false;
}

std::string BaseGrouping::findNearestDesiredName(hlim::NodePort nodePort)
{
    if (nodePort.node == nullptr)
        return "";
    
    if (dynamic_cast<hlim::Node_Signal*>(nodePort.node) != nullptr)
        return nodePort.node->getName();

    for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port))
        if (dynamic_cast<hlim::Node_Signal*>(driven.node) != nullptr)
            return driven.node->getName();
    
    return "";
}

void BaseGrouping::verifySignalsDisjoint()
{
    for (auto & s : m_inputs) {
        HCL_ASSERT(!m_outputs.contains(s));
        HCL_ASSERT(!m_localSignals.contains(s));
    } 
    for (auto & s : m_outputs) {
        HCL_ASSERT(!m_inputs.contains(s));
        HCL_ASSERT(!m_localSignals.contains(s));
    } 
    for (auto & s : m_localSignals) {
        HCL_ASSERT(!m_outputs.contains(s));
        HCL_ASSERT(!m_inputs.contains(s));
    } 
}


}
