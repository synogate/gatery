#include "BaseGrouping.h"

#include "AST.h"

#include "../../hlim/coreNodes/Node_Signal.h"

namespace mhdl::core::vhdl {

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


}
