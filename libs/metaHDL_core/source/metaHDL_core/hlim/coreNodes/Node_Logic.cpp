#include "Node_Logic.h"

namespace mhdl::core::hlim {

Node_Logic::Node_Logic(Op op) : Node(op==NOT?1:2, 1), m_op(op) 
{ 
    
}

void Node_Logic::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}

void Node_Logic::disconnectInput(size_t operand) 
{ 
    NodeIO::disconnectInput(operand); 
}

std::string Node_Logic::getTypeName() const 
{ 
    return "Logic"; 
}

void Node_Logic::assertValidity() const 
{ 
    
}

std::string Node_Logic::getInputName(size_t idx) const 
{ 
    return idx==0?"a":"b"; 
}

std::string Node_Logic::getOutputName(size_t idx) const 
{ 
    return "output"; 
}

    
void Node_Logic::updateConnectionType()
{
    auto lhs = getDriver(0);
    NodePort rhs;
    if (m_op != NOT)
        rhs = getDriver(1);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr) {
        if (rhs.node != nullptr) {
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
            MHDL_ASSERT_HINT(desiredConnectionType == rhs.node->getOutputConnectionType(rhs.port), "Support for differing types of input to logic node not yet implemented");
            //desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
        } else
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    } else if (rhs.node != nullptr)
        desiredConnectionType = rhs.node->getOutputConnectionType(rhs.port);
    
    setOutputConnectionType(0, desiredConnectionType);
}

}
