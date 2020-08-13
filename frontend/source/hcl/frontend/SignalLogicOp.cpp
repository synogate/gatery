#include "SignalLogicOp.h"

namespace hcl::core::frontend {

    SignalReadPort makeNode(hlim::Node_Logic::Op op, NormalizedWidthOperands ops)
    {
        HCL_DESIGNCHECK_HINT(op != hlim::Node_Logic::NOT, "Trying to perform a not operation with two operands.");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, ops.lhs);
        node->connectInput(1, ops.rhs);

        return SignalReadPort(node);
    }

    SignalReadPort makeNode(hlim::Node_Logic::Op op, const ElementarySignal& in)
    {
        HCL_DESIGNCHECK_HINT(op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, in.getReadPort());

        return SignalReadPort(node);
    }

}
