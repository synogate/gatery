#include "Bit.h"

namespace hcl::core::frontend {


Bit::Bit()
{
    HCL_ASSERT(m_node->isOrphaned());
    m_node->setConnectionType(getSignalType(1));
}

Bit::Bit(const Bit &rhs) {
    assign(rhs);
    m_node->setConnectionType(getSignalType(1));
}

Bit::Bit(const hlim::NodePort &port) : ElementarySignal(port) 
{ 
    m_node->setConnectionType(getSignalType(1));    
}

hlim::ConnectionType Bit::getSignalType(size_t width) const 
{
    HCL_ASSERT(width == 1);
    
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::BOOL;
    connectionType.width = 1;
    
    return connectionType;
}



}
