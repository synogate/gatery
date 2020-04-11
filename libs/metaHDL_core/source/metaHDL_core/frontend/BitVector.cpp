#include "BitVector.h"

namespace mhdl {
namespace core {    
namespace frontend {
    

BitVector::BitVector(unsigned width)
{ 
    resize(width); 
}

BitVector::BitVector(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType) : BaseBitVector<BitVector>(port, connectionType)
{ 
    
}

hlim::ConnectionType BitVector::getSignalType(unsigned width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::RAW;
    connectionType.width = width;
    
    return connectionType;
}


}
}
}
