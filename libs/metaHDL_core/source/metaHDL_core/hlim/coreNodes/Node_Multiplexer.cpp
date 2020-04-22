#include "Node_Multiplexer.h"


namespace mhdl::core::hlim {

void Node_Multiplexer::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(1+operand, port); 
    setOutputConnectionType(0, port.node->getOutputConnectionType(port.port)); 
}
        
}
