#pragma once
#include "../Node.h"

namespace hcl::core::hlim {

class Node_Arithmetic : public Node<Node_Arithmetic>
{
    public:
        enum Op {
            ADD,
            SUB,
            MUL,
            DIV,
            REM
        };

        Node_Arithmetic(Op op);

        void connectInput(size_t operand, const NodePort &port);
        void disconnectInput(size_t operand);

        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        inline Op getOp() const { return m_op; }

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const;

    protected:
        Op m_op;
        // extend or not, etc...

        void updateConnectionType();
};

}
