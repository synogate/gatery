#pragma once

#include "../Node.h"


namespace hcl::core::hlim {

class Node_MemReadPort;
class Node_MemWritePort;


/**
 * @brief Abstract memory node to represent e.g. BlockRams or LutRams
 * 
 * 
 * The Node_Memory can be accessed by connecting Node_MemReadPort and Node_MemWritePort as read and write ports.
 * Arbitrarily many read and write ports can be connected.
 * The Node_Memory serves as the entity that represents the stored information and groups the read and write ports together.
 * Writing through a write port is always synchronous with a one clock delay, reading is asynchronous but can be made synchronous
 * by adding registers to the data output.
 *
 * The connections are made through 1-bit wide connections between the nodes of type "ConnectionType::DEPENDENCY".
 * 
 */

class Node_Memory : public Node<Node_Memory>
{
    public:
        Node_Memory();

        std::size_t getSize() { return m_powerOnState.size(); }
        void setPowerOnState(sim::DefaultBitVectorState powerOnState);

        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

        virtual bool hasSideEffects() const override { return false; } // for now

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;
    protected:
        sim::DefaultBitVectorState m_powerOnState;
};

}