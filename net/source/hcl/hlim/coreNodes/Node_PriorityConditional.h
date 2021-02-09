#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace hcl::core::hlim {
    
class Node_PriorityConditional : public Node<Node_PriorityConditional>
{
    public:
        static size_t inputPortDefault() { return 0; }
        static size_t inputPortChoiceCondition(size_t choice) { return 1 + choice*2; }
        static size_t inputPortChoiceValue(size_t choice) { return 1 + choice*2 + 1; }
        
        Node_PriorityConditional() : Node(1, 1) { }
        
        void connectDefault(const NodePort &port);
        void disconnectDefault();

        void connectInput(size_t choice, const NodePort &condition, const NodePort &value);
        void addInput(const NodePort &condition, const NodePort &value);
        void disconnectInput(size_t choice);
        
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;        
        
        inline size_t getNumChoices() const { return (getNumInputPorts()-1)/2; }
        
        virtual std::string getTypeName() const;
        virtual void assertValidity() const;
        virtual std::string getInputName(size_t idx) const;
        virtual std::string getOutputName(size_t idx) const;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
};

}
