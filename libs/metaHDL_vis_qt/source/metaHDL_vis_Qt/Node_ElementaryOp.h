#pragma once
    
#include "Node.h"

#include <metaHDL_core/hlim/coreNodes/Node_Signal.h>

namespace mhdl::vis {
    
class Node_ElementaryOp : public Node
{
    public:
        Node_ElementaryOp(CircuitView *circuitView, core::hlim::BaseNode *hlimNode);

        enum { Type = UserType + 2 };
        int type() const override { return Type; }

    protected:
        core::hlim::BaseNode *m_hlimNode;
};


}
