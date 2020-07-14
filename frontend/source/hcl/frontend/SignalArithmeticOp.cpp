#include "SignalArithmeticOp.h"

namespace hcl::core::frontend {

BVec SignalArithmeticOp::operator()(const BVec &lhs, const BVec &rhs) {
    hlim::Node_Signal *lhsSignal = lhs.getNode();
    hlim::Node_Signal *rhsSignal = rhs.getNode();
    HCL_ASSERT(lhsSignal != nullptr);
    HCL_ASSERT(rhsSignal != nullptr);
    
    hlim::Node_Arithmetic *node = DesignScope::createNode<hlim::Node_Arithmetic>(m_op);
    node->recordStackTrace();
    node->connectInput(0, {.node = lhsSignal, .port = 0});
    node->connectInput(1, {.node = rhsSignal, .port = 0});

    return BVec({.node = node, .port = 0});
}


}
