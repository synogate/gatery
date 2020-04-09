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
    hlim::Node_Signal *signalNode = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();
    signalNode->recordStackTrace();
    m_port = &signalNode->getOutput(0);    
}

ElementarySignal::ElementarySignal(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType)
{
    hlim::Node_Signal *signalNode = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();
    signalNode->recordStackTrace();
    signalNode->setConnectionType(connectionType);
    port->connect(signalNode->getInput(0));
    m_port = &signalNode->getOutput(0);
}

void ElementarySignal::assign(const ElementarySignal &rhs) {
    hlim::Node_Signal *node = Scope::getCurrentNodeGroup()->addNode<hlim::Node_Signal>();    
    node->recordStackTrace();
    
    MHDL_ASSERT(rhs.m_port != nullptr);
    rhs.m_port->connect(node->getInput(0));    
    m_port = &node->getOutput(0);
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
