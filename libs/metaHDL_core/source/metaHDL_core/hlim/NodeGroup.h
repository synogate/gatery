#pragma once

#include "Node.h"

#include "../utils/StackTrace.h"

#include <vector>
#include <string>
#include <memory>

namespace mhdl::core::hlim {

class Node_Signal;
class Node_Register;
    
class NodeGroup
{
    public:
        enum GroupFlags {
            GRP_PROCEDURE   = 0x01,
            GRP_AREA        = 0x02,
            GRP_ENTITY      = 0x04,
        };
        
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        
        NodeGroup *addChildNodeGroup();
        
        inline const NodeGroup *getParent() const { return m_parent; }
        inline const std::string &getName() const { return m_name; }
        inline const std::vector<Node*> &getNodes() const { return m_nodes; }
        inline const std::vector<std::unique_ptr<NodeGroup>> &getChildren() const { return m_children; }


        bool isChildOf(const NodeGroup *other) const;
    protected:
        std::string m_name;
        
        std::vector<Node*> m_nodes;
        std::vector<std::unique_ptr<NodeGroup>> m_children;
        NodeGroup *m_parent = nullptr;
        
        utils::StackTrace m_stackTrace;
        
        friend class Node;
};

}
