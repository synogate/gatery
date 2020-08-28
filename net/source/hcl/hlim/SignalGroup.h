#pragma once

#include "../utils/StackTrace.h"

#include <vector>
#include <string>
#include <memory>

namespace hcl::core::hlim {

class Node_Signal;
    
class SignalGroup
{
    public:
        enum class GroupType {
            ARRAY      = 0x01,
            STRUCT     = 0x02,
        };
        
        SignalGroup(GroupType groupType);
        ~SignalGroup();
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setComment(std::string comment) { m_comment = std::move(comment); }
        
        SignalGroup *addChildSignalGroup(GroupType groupType);
        
        inline const SignalGroup *getParent() const { return m_parent; }
        inline const std::string &getName() const { return m_name; }
        inline const std::string &getComment() const { return m_comment; }
        inline const std::vector<Node_Signal*> &getNodes() const { return m_nodes; }
        inline const std::vector<std::unique_ptr<SignalGroup>> &getChildren() const { return m_children; }

        bool isChildOf(const SignalGroup *other) const;
        
        inline GroupType getGroupType() const { return m_groupType; }
    protected:
        std::string m_name;
        std::string m_comment;
        GroupType m_groupType;
        
        std::vector<Node_Signal*> m_nodes;
        std::vector<std::unique_ptr<SignalGroup>> m_children;
        SignalGroup *m_parent = nullptr;
        
        utils::StackTrace m_stackTrace;
        
        friend class Node_Signal;
};

}
