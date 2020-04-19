#include "Integers.h"

namespace mhdl::core::frontend {
    
hlim::ConnectionType UnsignedInteger::getSignalType(size_t width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::UNSIGNED;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = width;
    
    return connectionType;
}


hlim::ConnectionType SignedInteger::getSignalType(size_t width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::SIGNED_2COMPLEMENT;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = width;
    
    return connectionType;
}



}
