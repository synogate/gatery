#include "SignalArithmeticOp.h"

namespace hcl::core::frontend {

BVec SignalArithmeticOp::operator()(const BVec &lhs, const BVec &rhs) {
    hlim::Node_Arithmetic *node = DesignScope::createNode<hlim::Node_Arithmetic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, lhs.getReadPort());
    node->connectInput(1, rhs.getReadPort());

    return BVec({.node = node, .port = 0});
}


}
