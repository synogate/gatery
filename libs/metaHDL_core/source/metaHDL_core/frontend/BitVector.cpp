#include "BitVector.h"

namespace mhdl::core::frontend {
    

BitVector::BitVector(size_t width)
{ 
    resize(width); 
}

BitVector::BitVector(const hlim::NodePort &port) : BaseBitVector<BitVector>(port)
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
