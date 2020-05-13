#include "Node_Register.h"

namespace mhdl::core::hlim {

Node_Register::Node_Register() : Node(NUM_INPUTS, 1) 
{ 
    m_clocks.resize(1);
}

void Node_Register::connectInput(Input input, const NodePort &port) 
{ 
    NodeIO::connectInput(input, port); 
    if (input == DATA || input == RESET_VALUE) 
        setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Register::setClock(BaseClock *clk)
{
    attachClock(clk, 0);
}

void Node_Register::setReset(std::string resetName)
{
    m_resetName = std::move(resetName);
}


std::string Node_Register::getTypeName() const
{ 
    return "Register"; 
}

void Node_Register::assertValidity() const 
{ 
    
}

std::string Node_Register::getInputName(size_t idx) const 
{ 
    switch (idx) { 
        case DATA: return "data_in"; 
        case RESET_VALUE: return "reset_value"; 
        case ENABLE: return "enable"; 
        default: 
            return "INVALID"; 
    } 
}

std::string Node_Register::getOutputName(size_t idx) const 
{ 
    return "data_out";
}

size_t Node_Register::getInternalStateSize() const 
{
    return getOutputConnectionType(0).width;
}


}
