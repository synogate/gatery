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
        
        template<typename NodeType, typename... Args>
        NodeType *createNode(Args&&... args);        
        
        inline NodeGroup *getRootNodeGroup() { return m_root.get(); }
        inline const NodeGroup *getRootNodeGroup() const { return m_root.get(); }
    protected:
        std::vector<std::unique_ptr<Node>> m_nodes;
        std::unique_ptr<NodeGroup> m_root;
};


template<typename NodeType, typename... Args>
NodeType *Circuit::createNode(Args&&... args) {
    m_nodes.push_back(std::make_unique<NodeType>(this, std::forward<Args>(args)...));
    return (NodeType *) m_nodes.back().get();
}


}
