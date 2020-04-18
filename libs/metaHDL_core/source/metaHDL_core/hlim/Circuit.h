#pragma once

#include "NodeGroup.h"
#include "Node.h"
#include "ConnectionType.h"

#include <vector>
#include <memory>

namespace mhdl::core::hlim {


class Circuit
{
    public:
        Circuit();
        
        inline NodeGroup *getRootNodeGroup() { return m_root.get(); }
        inline const NodeGroup *getRootNodeGroup() const { return m_root.get(); }
    protected:
        std::vector<Node*> m_inputs;
        std::vector<Node*> m_outputs;
        std::unique_ptr<NodeGroup> m_root;
};


}
