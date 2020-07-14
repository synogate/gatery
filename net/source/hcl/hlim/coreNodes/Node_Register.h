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
        
        void connectInput(Input input, const NodePort &port);
        inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }
        
        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
        virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const override;
        
        void setClock(Clock *clk);
        void setReset(std::string resetName);
        
        inline const std::string &getResetName() { return m_resetName; }
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;
    protected:
        std::string m_resetName;
};

}
