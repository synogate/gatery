#pragma once
    
#include "Node.h"

#include <metaHDL_core/hlim/NodeGroup.h>
#include <metaHDL_core/hlim/NodeIO.h>

namespace hcl::vis {
    
class Node_Entity : public Node
{
    public:
        Node_Entity(CircuitView *circuitView, core::hlim::NodeGroup *nodeGroup);

        enum { Type = UserType + 2 };
        int type() const override { return Type; }
    protected:
        core::hlim::NodeGroup *m_hlimNodeGroup;
};


}
