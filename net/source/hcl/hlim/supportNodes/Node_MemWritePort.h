#pragma once

#include "../Node.h"


namespace hcl::core::hlim {

class Node_Memory;

class Node_MemWritePort : public Node<Node_MemWritePort>
{
    public:
        enum class Inputs {
            memory,
            enable,
            address,
            data,
            count
        };

        Node_MemWritePort(std::size_t bitWidth);

        void connectMemory(Node_Memory *memory);
        void disconnectMemory();

        Node_Memory *getMemory();
        void connectEnable(const NodePort &output);
        void connectAddress(const NodePort &output);
        void connectData(const NodePort &output);

        void setClock(Clock* clk);


        virtual bool hasSideEffects() const override;

        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;        

        inline size_t getBitWidth() const { return m_bitWidth; }
    protected:
        friend class Node_Memory;
        std::size_t m_bitWidth;
};

}