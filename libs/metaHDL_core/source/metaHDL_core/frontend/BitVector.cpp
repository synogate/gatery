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

hlim::ConnectionType BitVector::getSignalType() const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::RAW;
    connectionType.width = m_width;
    
    return connectionType;
}


}
}
}
