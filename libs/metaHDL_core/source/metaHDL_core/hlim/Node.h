#pragma once

#include "NodeIO.h"
#include "NodeVisitor.h"

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
    
class BaseNode : public NodeIO
{
    public:
        BaseNode();
        BaseNode(size_t numInputs, size_t numOutputs);
        virtual ~BaseNode();

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

        virtual void visit(NodeVisitor &visitor) = 0;
        virtual void visit(ConstNodeVisitor &visitor) const = 0;
    protected:
        std::string m_name;
        utils::StackTrace m_stackTrace;
        NodeGroup *m_nodeGroup = nullptr;
};

template<class FinalType>
class Node : public BaseNode
{
    public:
        Node() : BaseNode() { }
        Node(size_t numInputs, size_t numOutputs) : BaseNode(numInputs, numOutputs) { }
        
        virtual void visit(NodeVisitor &visitor) override { visitor(*static_cast<FinalType*>(this)); }
        virtual void visit(ConstNodeVisitor &visitor) const override { visitor(*static_cast<const FinalType*>(this)); }
};


}
