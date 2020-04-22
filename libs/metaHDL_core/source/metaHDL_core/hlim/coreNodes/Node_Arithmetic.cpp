#include "Node_Arithmetic.h"

namespace mhdl::core::hlim {

Node_Arithmetic::Node_Arithmetic(Op op) : Node(2, 1), m_op(op) 
{ 
    
}

void Node_Arithmetic::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}

void Node_Arithmetic::updateConnectionType()
{
    auto lhs = getDriver(0);
    auto rhs = getDriver(1);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr) {
        if (rhs.node != nullptr) {
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
            desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
        } else
            desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    } else if (rhs.node != nullptr)
        desiredConnectionType = rhs.node->getOutputConnectionType(rhs.port);
    
    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Arithmetic::disconnectInput(size_t operand) 
{ 
    NodeIO::disconnectInput(operand); 
}

std::string Node_Arithmetic::getTypeName() const 
{ 
    return "Arithmetic"; 
}

void Node_Arithmetic::assertValidity() const 
{ 
    
}

std::string Node_Arithmetic::getInputName(size_t idx) const 
{ 
    return idx==0?"a":"b"; 
}

std::string Node_Arithmetic::getOutputName(size_t idx) const 
{ 
    return "out";
}

        
}
