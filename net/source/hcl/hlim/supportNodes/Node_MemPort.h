#pragma once

#include "../Node.h"

namespace hcl::core::hlim {

class Node_Memory;

class Node_MemPort : public Node<Node_MemPort>
{
    public:
        enum class Inputs {
            memory,
            enable,
            wrEnable,
            address,
            wrData,
            orderAfter,
            count
        };

        enum class Outputs {
            rdData,
            orderBefore,
            count
        };

        Node_MemPort(std::size_t bitWidth);

        void connectMemory(Node_Memory *memory);
        void disconnectMemory();

        Node_Memory *getMemory();
        void connectEnable(const NodePort &output);
        void connectWrEnable(const NodePort &output);
        void connectAddress(const NodePort &output);
        void connectWrData(const NodePort &output);
        void orderAfter(Node_MemPort *port);
        bool isOrderedAfter(Node_MemPort *port) const;
        bool isOrderedBefore(Node_MemPort *port) const;

        void setClock(Clock* clk);

        bool isReadPort();
        bool isWritePort();

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