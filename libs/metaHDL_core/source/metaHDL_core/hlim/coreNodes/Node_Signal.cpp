#include "Node_Signal.h"

#include "../../utils/Exceptions.h"

namespace mhdl::core::hlim {
    
void Node_Signal::setConnectionType(const ConnectionType &connectionType)
{
    MHDL_ASSERT_HINT(getDirectlyDriven(0).empty(), "Can only change or set the connection type on unconnected signal nodes!");
    setOutputConnectionType(0, connectionType);
}

void Node_Signal::connectInput(const NodePort &nodePort)
{
    NodeIO::connectInput(0, nodePort);
}

void Node_Signal::disconnectInput()
{
    NodeIO::disconnectInput(0);
}


}
