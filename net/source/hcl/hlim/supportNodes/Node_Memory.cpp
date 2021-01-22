#include "Node_Memory.h"

#include "Node_MemReadPort.h"
#include "Node_MemWritePort.h"

namespace hcl::core::hlim {

    Node_Memory::Node_Memory()
    {
        resizeOutputs(1);
        setOutputConnectionType(0, {.interpretation = ConnectionType::DEPENDENCY, .width=1});
    }

    void Node_Memory::setPowerOnState(sim::DefaultBitVectorState powerOnState)
    {
        m_powerOnState = std::move(powerOnState);
    }

    void Node_Memory::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
    {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], 1);
    }

    void Node_Memory::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
    {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], 1);
    }


    std::string Node_Memory::getTypeName() const
    {
        return "memory";
    }

    void Node_Memory::assertValidity() const
    {
    }

    std::string Node_Memory::getInputName(size_t idx) const
    {
        return "";
    }

    std::string Node_Memory::getOutputName(size_t idx) const
    {
        return "memory_ports";
    }

    std::vector<size_t> Node_Memory::getInternalStateSizes() const
    {
        return { m_powerOnState.size() };
    }

}