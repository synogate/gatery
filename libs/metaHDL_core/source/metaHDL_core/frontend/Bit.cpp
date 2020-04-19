#include "Bit.h"

namespace mhdl::core::frontend {


Bit::Bit()
{
    MHDL_ASSERT(m_node->isOrphaned());
    m_node->setConnectionType(getSignalType(1));
}

Bit::Bit(const Bit &rhs) {
    assign(rhs);
}

Bit::Bit(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : ElementarySignal(port, connectionType) 
{ 
    
}

hlim::ConnectionType Bit::getSignalType(size_t width) const 
{
    MHDL_ASSERT(width == 1);
    
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::BOOL;
    connectionType.width = 1;
    
    return connectionType;
}



}
