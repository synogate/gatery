#include "Integers.h"

namespace mhdl {
namespace core {    
namespace frontend {
    
hlim::ConnectionType UnsignedInteger::getSignalType() const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::UNSIGNED;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = m_width;
    
    return connectionType;
}


hlim::ConnectionType SignedInteger::getSignalType() const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::SIGNED_2COMPLEMENT;
    connectionType.fixedPoint_denominator = 1;
    connectionType.width = m_width;
    
    return connectionType;
}



}
}
}
