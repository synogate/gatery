#include "SignalLogicOp.h"

namespace mhdl {
namespace core {    
namespace frontend {

hlim::ConnectionType SignalLogicOp::getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs)
{
    return lhs; /// @todo
}

}
}
}
