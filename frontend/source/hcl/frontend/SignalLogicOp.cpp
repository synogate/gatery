#include "SignalLogicOp.h"

namespace hcl::core::frontend {

    hlim::NodePort makeNode(hlim::Node_Logic::Op op, const SignalPort& lhs, const SignalPort& rhs)
    {
        HCL_DESIGNCHECK_HINT(op != hlim::Node_Logic::NOT, "Trying to perform a not operation with two operands.");
        HCL_DESIGNCHECK_HINT(lhs.getConnType() == lhs.getConnType(), "Can only perform logic operations on operands of same type (e.g. width).");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, lhs.getReadPort());
        node->connectInput(1, rhs.getReadPort());

        return { .node = node, .port = 0ull };
    }

    hlim::NodePort makeNode(hlim::Node_Logic::Op op, const SignalPort& in)
    {
        HCL_DESIGNCHECK_HINT(op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, in.getReadPort());

        return { .node = node, .port = 0ull };
    }

    hlim::NodePort makeNodeEqWidth(hlim::Node_Logic::Op op, const SignalPort& lhs, const BitSignalPort& rhs)
    {
        return makeNode(op, lhs, BVecSignalPort{ Bit{rhs}.sext(lhs.getWidth()) });
    }

}
