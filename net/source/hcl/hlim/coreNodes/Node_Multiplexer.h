#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace hcl::core::hlim {
    
class Node_Multiplexer : public Node<Node_Multiplexer>
{
    public:
        Node_Multiplexer(size_t numMultiplexedInputs) : Node(1+numMultiplexedInputs, 1) { }
        
        inline void connectSelector(const NodePort &port) { NodeIO::connectInput(0, port); }
        inline void disconnectSelector() { NodeIO::disconnectInput(0); }

        void connectInput(size_t operand, const NodePort &port);
        inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(1+operand); }

        virtual void simulateEvaluate(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
        
        virtual std::string getTypeName() const override { return "mux"; }
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { if (idx == 0) return "select"; return (boost::format("in_%d") % (idx-1)).str(); }
        virtual std::string getOutputName(size_t idx) const override { return "out"; }
};

}
