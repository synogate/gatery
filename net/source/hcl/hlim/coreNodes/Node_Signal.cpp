#include "Node_Signal.h"

#include "../../utils/Exceptions.h"

namespace hcl::core::hlim {
    
void Node_Signal::setConnectionType(const ConnectionType &connectionType)
{
    setOutputConnectionType(0, connectionType);
}

void Node_Signal::connectInput(const NodePort &nodePort)
{
    auto paramType = nodePort.node->getOutputConnectionType(nodePort.port);
    auto myType = getOutputConnectionType(0);
    HCL_ASSERT_HINT(paramType == myType, "The connection type of the node that is being connected does not match the connection type of the signal");
    NodeIO::connectInput(0, nodePort);
}

void Node_Signal::disconnectInput()
{
    NodeIO::disconnectInput(0);
}


}
