#include "Bit.h"

namespace mhdl {
namespace core {    
namespace frontend {


Bit::Bit()
{
    hlim::Node_Signal *signalNode = dynamic_cast<hlim::Node_Signal*>(m_port->node);
    MHDL_ASSERT(signalNode != nullptr);
    MHDL_ASSERT(signalNode->isOrphaned());
    
    signalNode->setConnectionType(getSignalType());    
}

Bit::Bit(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : ElementarySignal(port, connectionType) 
{ 
    
}

hlim::ConnectionType Bit::getSignalType() const 
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::BOOL;
    connectionType.width = 1;
    
    return connectionType;
}



}
}
}
