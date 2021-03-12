#pragma once

#include "../Node.h"

#include <boost/format.hpp>

namespace hcl::core::hlim {

    class Node_Register : public Node<Node_Register>
    {
    public:
        enum Input {
            DATA,
            RESET_VALUE,
            ENABLE,
            NUM_INPUTS
        };
        enum Internal {
            INT_DATA,
            INT_ENABLE,
            NUM_INTERNALS
        };

        Node_Register();

        void connectInput(Input input, const NodePort& port);
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }

        virtual void simulateReset(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* outputOffsets) const override;
        virtual void simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const override;
        virtual void simulateAdvance(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* outputOffsets, size_t clockPort) const override;

        void setClock(Clock* clk);

        bool hasSideEffects() const override { return hasRef(); }

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;

        void setConditionId(size_t id) { m_conditionId = id; }
        size_t getConditionId() const { return m_conditionId; }

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const override;
    protected:
        size_t m_conditionId = 0;
    };

}
