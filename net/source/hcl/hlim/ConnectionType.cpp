#include "ConnectionType.h"

namespace hcl::core::hlim {


bool ConnectionType::operator==(const ConnectionType &rhs) const
{
    if (rhs.interpretation != interpretation) return false;
    if (rhs.width != width) return false;

    return true;
}
    
#if 0

size_t CompoundConnectionType::getTotalWidth() const 
{
    size_t sum = 0;
    for (const auto &sub : m_subConnections)
        sum += sub.type->getTotalWidth();
    return sum;
}

#endif

}

