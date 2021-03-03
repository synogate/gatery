#pragma once

#include "../Node.h"

namespace hcl::core::hlim {


class Node_SignalGenerator : public Node<Node_SignalGenerator>
{
    public:
        Node_SignalGenerator(Clock *clk);
        
        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;

    protected:
        void setOutputs(const std::vector<ConnectionType> &connections);
        void resetDataDefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const;
        void resetDataUndefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const;
        
        virtual void produceSignals(sim::DefaultBitVectorState &state, const size_t *outputOffsets, size_t clockTick) const = 0;
};

}
