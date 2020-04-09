#include "SignalArithmeticOp.h"

namespace mhdl {
namespace core {    
namespace frontend {

hlim::ConnectionType SignalArithmeticOp::getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs)
{
    return lhs; /// @todo
}

}
}
}
