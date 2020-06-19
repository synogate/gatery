#include "SignalBitshiftOp.h"

namespace hcl::core::frontend {

hlim::ConnectionType SignalBitShiftOp::getResultingType(const hlim::ConnectionType &operand) {
    return operand;
}

}
