#pragma once

#include "Node.h"

#include "../utils/StackTrace.h"

#include <vector>
#include <string>
#include <memory>

namespace mhdl {
namespace core {
namespace hlim {

class NodeGroup
{
    public:
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        
        NodeGroup *addChildNodeGroup();
        
        template<typename NodeType, typename... Args>
        NodeType *addNode(Args&&... args);
    protected:
        std::string m_name;
        
        std::vector<std::unique_ptr<Node>> m_nodes;
        std::vector<std::unique_ptr<NodeGroup>> m_children;
        NodeGroup *m_parent = nullptr;
        
        utils::StackTrace m_stackTrace;
};





template<typename NodeType, typename... Args>
NodeType *NodeGroup::addNode(Args&&... args) {
    m_nodes.push_back(std::make_unique<NodeType>(this, std::forward<Args>(args)...));
    return (NodeType *) m_nodes.back().get();
}

}
}
}
