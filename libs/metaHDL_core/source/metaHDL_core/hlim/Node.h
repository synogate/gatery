#pragma once

#include "NodeIO.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"

#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <vector>
#include <set>
#include <string>

namespace mhdl::core::hlim {

class NodeGroup;
    
class Node : public NodeIO
{
    public:
        Node();
        Node(size_t numInputs, size_t numOutputs);
        virtual ~Node() { }

        virtual std::string getTypeName() const = 0;
        virtual void assertValidity() const = 0;
        virtual std::string getInputName(size_t idx) const = 0;
        virtual std::string getOutputName(size_t idx) const = 0;
        
        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }

        inline void setName(std::string name) { m_name = std::move(name); }
        inline const std::string &getName() const { return m_name; }
        
        bool isOrphaned() const;
        
        const NodeGroup *getGroup() const { return m_nodeGroup; }
        NodeGroup *getGroup() { return m_nodeGroup; }
        
        void moveToGroup(NodeGroup *group);
    protected:
        std::string m_name;
        utils::StackTrace m_stackTrace;
        NodeGroup *m_nodeGroup = nullptr;
        
};

}
