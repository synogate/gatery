#pragma once

#include "Node.h"

#include "../utils/StackTrace.h"

#include <vector>
#include <string>
#include <memory>

namespace hcl::core::hlim {

class Node_Signal;
class Node_Register;
    
class NodeGroup
{
    public:
        enum class GroupType {
            ENTITY      = 0x01,
            AREA        = 0x02,
        };
        
        NodeGroup(GroupType groupType);
        ~NodeGroup();
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setComment(std::string comment) { m_comment = std::move(comment); }
        
        NodeGroup *addChildNodeGroup(GroupType groupType);
        
        inline const NodeGroup *getParent() const { return m_parent; }
        inline const std::string &getName() const { return m_name; }
        inline const std::string &getComment() const { return m_comment; }
        inline const std::vector<BaseNode*> &getNodes() const { return m_nodes; }
        inline const std::vector<std::unique_ptr<NodeGroup>> &getChildren() const { return m_children; }

        bool isChildOf(const NodeGroup *other) const;
        
        inline GroupType getGroupType() const { return m_groupType; }
    protected:
        std::string m_name;
        std::string m_comment;
        GroupType m_groupType;
        
        std::vector<BaseNode*> m_nodes;
        std::vector<std::unique_ptr<NodeGroup>> m_children;
        NodeGroup *m_parent = nullptr;
        
        utils::StackTrace m_stackTrace;
        
        friend class BaseNode;
};

}
