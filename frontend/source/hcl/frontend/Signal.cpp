#include "Signal.h"
#include "Scope.h"
#include "ConditionalScope.h"

#include <hcl/utils/Exceptions.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>

#include <iostream>

namespace hcl::core::frontend {


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

ElementarySignal::~ElementarySignal()
{
    if (m_conditionalScope != nullptr)
        m_conditionalScope->unregisterSignal(this);
}


void ElementarySignal::assignConditionalScopeMuxOutput(const hlim::NodePort &port, ConditionalScope *parentScope)
{
    std::string oldName = m_node->getName();
    
    m_node = DesignScope::createNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
    if (port.node != nullptr)
        m_node->setConnectionType(port.node->getOutputConnectionType(port.port));
    m_node->connectInput(port);
    
    setName(oldName);    
    m_conditionalScope = parentScope;
}

void ElementarySignal::assign(const ElementarySignal &rhs) {
    
    hlim::NodePort previousOutput = {.node = m_node, .port = 0ull};
    
    std::string oldName = m_node->getName();
    
    m_node = DesignScope::createNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
    m_node->setConnectionType(rhs.m_node->getOutputConnectionType(0));
    m_node->connectInput({.node = rhs.m_node, .port = 0});
    
    if (oldName.empty())
        setName(rhs.m_node->getName());
    else 
        setName(oldName);
    
    if (ConditionalScope::get() != nullptr) {
        ConditionalScope::get()->registerConditionalAssignment(this, previousOutput);
        m_conditionalScope = ConditionalScope::get();
    }
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
