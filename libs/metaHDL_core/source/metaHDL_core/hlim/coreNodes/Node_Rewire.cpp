#include "Node_Rewire.h"

namespace mhdl::core::hlim {

bool Node_Rewire::RewireOperation::isBitExtract(size_t& bitIndex) const
{
    if (ranges.size() == 1) {
        if (ranges[0].subwidth == 1 &&
            ranges[0].source == OutputRange::INPUT &&
            ranges[0].inputIdx == 0) {
            
            bitIndex = ranges[0].inputOffset;
            return true;
        }
    }
    return false;
}

Node_Rewire::Node_Rewire(size_t numInputs) : Node(numInputs, 1)
{
    
}

void Node_Rewire::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}


void Node_Rewire::updateConnectionType()
{
    auto lhs = getDriver(0);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr)
        desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    
    desiredConnectionType.width = 0;
    for (auto r : m_rewireOperation.ranges)
        desiredConnectionType.width += r.subwidth;

    setOutputConnectionType(0, desiredConnectionType);
}


}
