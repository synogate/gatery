#include "BitVector.h"

namespace mhdl::core::frontend {
    

BitVector::BitVector(size_t width)
{ 
    resize(width); 
}

BitVector::BitVector(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : BaseBitVector<BitVector>(port, connectionType)
{ 
    
}

hlim::ConnectionType BitVector::getSignalType(size_t width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::RAW;
    connectionType.width = width;
    
    return connectionType;
}


}
