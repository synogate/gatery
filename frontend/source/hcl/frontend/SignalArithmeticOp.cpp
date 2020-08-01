#include "SignalArithmeticOp.h"

namespace hcl::core::frontend {

hlim::NodePort makeNode(hlim::Node_Arithmetic op, BVecSignalPort lhs, BVecSignalPort rhs)
{
    hlim::Node_Arithmetic* node = DesignScope::createNode<hlim::Node_Arithmetic>(op);
    node->recordStackTrace();
    node->connectInput(0, lhs.getReadPort());
    node->connectInput(1, rhs.getReadPort());

    return { .node = node, .port = 0 };
}

}
