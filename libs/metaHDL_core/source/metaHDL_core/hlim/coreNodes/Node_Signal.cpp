#include "Node_Signal.h"

#include "../../utils/Exceptions.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
void Node_Signal::setConnectionType(const ConnectionType &connectionType)
{
    MHDL_ASSERT_HINT(isOrphaned(), "Can only change or set the connection type on unconnected signal nodes!");
    
    m_connectionType = connectionType;
}

}
}
}
