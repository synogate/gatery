#include "SignalBitshiftOp.h"

namespace mhdl {
namespace core {    
namespace frontend {

hlim::ConnectionType SignalBitShiftOp::getResultingType(const hlim::ConnectionType &operand) {
    return operand;
}

}
}
}
