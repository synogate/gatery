#include "SignalArithmeticOp.h"

namespace mhdl::core::frontend {

hlim::ConnectionType SignalArithmeticOp::getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs)
{
    hlim::ConnectionType type = lhs;
    type.width = std::max(lhs.width, rhs.width);
    return lhs; /// @todo
}

}
