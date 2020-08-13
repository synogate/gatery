#include "SignalArithmeticOp.h"

namespace hcl::core::frontend {


	SignalReadPort makeNode(hlim::Node_Arithmetic op, NormalizedWidthOperands ops)
	{
		hlim::Node_Arithmetic* node = DesignScope::createNode<hlim::Node_Arithmetic>(op);
		node->recordStackTrace();
		node->connectInput(0, ops.lhs);
		node->connectInput(1, ops.rhs);
		
		return SignalReadPort(node);
	}

}
