#pragma once
#include "../Node.h"

namespace mhdl::core::hlim {
    
class Node_Compare : public Node<Node_Compare>
{
    public:
        enum Op {
            EQ,
            NEQ,
            LT,
            GT,
            LEQ,
            GEQ
        };
        
        Node_Compare(Op op);
        
        inline void connectInput(size_t operand, const NodePort &port) { NodeIO::connectInput(operand, port); }
        inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(operand); }        
        
        virtual void simulateEvaluate(sim::DefaultBitVectorState &state, const size_t internalOffset, const size_t *inputOffsets, const size_t *outputOffsets) const override;        
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;
    protected:
        Op m_op;
};

}
