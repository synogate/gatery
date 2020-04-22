#include "Node_Compare.h"


namespace mhdl::core::hlim {

Node_Compare::Node_Compare() : Node(2, 1)
{
    ConnectionType conType;
    conType.width = 1;
    conType.interpretation = ConnectionType::BOOL;
    setOutputConnectionType(0, conType);
}

}
