#include "Integers.h"

namespace mhdl {
namespace core {    
namespace frontend {
    
hlim::ConnectionType UnsignedInteger::getSignalType(unsigned width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::UNSIGNED;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = width;
    
    return connectionType;
}


hlim::ConnectionType SignedInteger::getSignalType(unsigned width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::SIGNED_2COMPLEMENT;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = width;
    
    return connectionType;
}



}
}
}
