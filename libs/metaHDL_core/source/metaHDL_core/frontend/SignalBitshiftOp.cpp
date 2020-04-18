#include "SignalBitshiftOp.h"

namespace mhdl::core::frontend {

hlim::ConnectionType SignalBitShiftOp::getResultingType(const hlim::ConnectionType &operand) {
    return operand;
}

}
