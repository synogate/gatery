#include "Signal.h"
#include "Scope.h"

#include "../utils/Exceptions.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <iostream>

namespace mhdl {
namespace core {    
namespace frontend {


ElementarySignal::ElementarySignal()
{
    m_node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();
    m_node->recordStackTrace();
}

ElementarySignal::ElementarySignal(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType)
{
    m_node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();
    m_node->recordStackTrace();
    m_node->setConnectionType(connectionType);
    port->connect(m_node->getInput(0));
}

void ElementarySignal::assign(const ElementarySignal &rhs) {
    m_node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();    
    m_node->recordStackTrace();
    m_node->setConnectionType(rhs.m_node->getConnectionType());
    
    MHDL_ASSERT(rhs.m_node != nullptr);
    rhs.m_node->getOutput(0).connect(m_node->getInput(0));    
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
}
}
