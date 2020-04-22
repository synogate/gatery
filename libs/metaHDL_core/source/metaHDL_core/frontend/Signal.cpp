#include "Signal.h"
#include "Scope.h"

#include "../utils/Exceptions.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <iostream>

namespace mhdl::core::frontend {


ElementarySignal::ElementarySignal()
{
    m_node = DesignScope::createNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
}

ElementarySignal::ElementarySignal(const hlim::NodePort &port)
{
    m_node = DesignScope::createNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
    m_node->setConnectionType(port.node->getOutputConnectionType(port.port));
    m_node->connectInput(port);
}

void ElementarySignal::assign(const ElementarySignal &rhs) {
    std::string oldName = m_node->getName();
    
    m_node = DesignScope::createNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
    m_node->setConnectionType(rhs.m_node->getOutputConnectionType(0));
    m_node->connectInput({.node = rhs.m_node, .port = 0});
    
    if (oldName.empty())
        setName(rhs.m_node->getName());
    else 
        setName(oldName);    
}


/*
    
CompoundSignal &CompoundSignal::operator=(const CompoundSignal &rhs)
{
    std::cout << "assignment CompoundSignal:" << getSignalTypeName() << std::endl;
    return *this;
}

void CompoundSignal::registerSignal(const std::string &name, BaseSignal &signal)
{
    signal.setName(name);
    m_subSignals.push_back(&signal);
}
*/

}
