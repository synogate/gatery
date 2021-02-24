#pragma once

#include "NodeIO.h"
#include "NodeVisitor.h"


#include "../simulation/BitVectorState.h"
#include "../simulation/SimulatorCallbacks.h"

#include "../utils/StackTrace.h"
#include "../utils/LinkedList.h"
#include "../utils/Exceptions.h"
#include "../utils/CppTools.h"

#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <vector>
#include <set>
#include <string>

namespace hcl::core::hlim {

class NodeGroup;
class Clock;

class BaseNode : public NodeIO
{
    public:
        BaseNode();
        BaseNode(size_t numInputs, size_t numOutputs);
        virtual ~BaseNode();

        void addRef() { m_refCounter++; }
        void removeRef() { HCL_ASSERT(m_refCounter > 0); m_refCounter--; }
        bool hasRef() const { return m_refCounter > 0; }

        virtual std::string getTypeName() const = 0;
        virtual void assertValidity() const = 0;
        virtual std::string getInputName(size_t idx) const = 0;
        virtual std::string getOutputName(size_t idx) const = 0;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const = 0;

        /// Returns a list of word sizes of all the words of internal state that the node needs for simulation.
        virtual std::vector<size_t> getInternalStateSizes() const { return {}; }
        /// Returns a list of nodes and word indices to refer to the internal state words of other nodes that this node needs to access during simulation.
        virtual std::vector<std::pair<BaseNode*, size_t>> getReferencedInternalStateSizes() const { return {}; }

        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const { }
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const { }
        virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const { }

        inline void recordStackTrace() { m_stackTrace.record(10, 1); }
        inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }

        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setComment(std::string comment) { m_comment = std::move(comment); }
        inline const std::string &getName() const { return m_name; }
        inline const std::string &getComment() const { return m_comment; }

        bool isOrphaned() const;
        virtual bool hasSideEffects() const;
        virtual bool isCombinatorial() const;

        const NodeGroup *getGroup() const { return m_nodeGroup; }
        NodeGroup *getGroup() { return m_nodeGroup; }

        inline const std::vector<Clock*> &getClocks() const { return m_clocks; }

        void moveToGroup(NodeGroup *group);

        virtual void visit(NodeVisitor &visitor) = 0;
        virtual void visit(ConstNodeVisitor &visitor) const = 0;

        void attachClock(Clock *clk, size_t clockPort);
        void detachClock(size_t clockPort);

        /// Returns an id that is unique to this node within the circuit.
        /// @details The id order is preserved when subnets are copied and always reflects creation order.
        inline std::uint64_t getId() const { return m_nodeId; }

        void setId(std::uint64_t id, utils::RestrictTo<Circuit>) { m_nodeId = id; }
    protected:
        std::uint64_t m_nodeId = ~0ull;

        std::string m_name;
        std::string m_comment;
        utils::StackTrace m_stackTrace;
        NodeGroup *m_nodeGroup = nullptr;
        std::vector<Clock*> m_clocks;

        size_t m_refCounter = 0;


        void copyBaseToClone(BaseNode *copy) const;
};


template<class FinalType>
class Node : public BaseNode
{
    public:
        Node() : BaseNode() { }
        Node(size_t numInputs, size_t numOutputs) : BaseNode(numInputs, numOutputs) { }

        virtual void visit(NodeVisitor &visitor) override { visitor(*static_cast<FinalType*>(this)); }
        virtual void visit(ConstNodeVisitor &visitor) const override { visitor(*static_cast<const FinalType*>(this)); }


    protected:

};


}
