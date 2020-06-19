#include "SignalLogicOp.h"

namespace hcl::core::frontend {

hlim::ConnectionType SignalLogicOp::getResultingType(const hlim::ConnectionType &lhs, const hlim::ConnectionType &rhs)
{
    return lhs; /// @todo
}

}
