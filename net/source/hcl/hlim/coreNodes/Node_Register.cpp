#include "Node_Register.h"
#include "Node_Constant.h"

namespace hcl::core::hlim {

Node_Register::Node_Register() : Node(NUM_INPUTS, 1) 
{ 
    m_clocks.resize(1);
    setOutputType(0, OUTPUT_LATCHED);
}

void Node_Register::connectInput(Input input, const NodePort &port) 
{ 
    NodeIO::connectInput(input, port); 
    if (port.node != nullptr)
        if (input == DATA || input == RESET_VALUE) 
            setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Register::setClock(Clock *clk)
{
    attachClock(clk, 0);
}

void Node_Register::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
    auto resetDriver = getNonSignalDriver(RESET_VALUE);
    if (resetDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, internalOffsets[0], getOutputConnectionType(0).width, false);
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    Node_Constant *constNode = dynamic_cast<Node_Constant *>(resetDriver.node);
    HCL_ASSERT_HINT(constNode != nullptr, "Constant value propagation is not yet implemented, so for simulation the register reset value must be connected to a constant node via signals only!");
    
    state.insert(constNode->getValue(), outputOffsets[0]);
}

void Node_Register::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    if (inputOffsets[DATA] == ~0ull)
        state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[INT_DATA], getOutputConnectionType(0).width);
    else
        state.copyRange(internalOffsets[INT_DATA], state, inputOffsets[DATA], getOutputConnectionType(0).width);
    
    if (inputOffsets[ENABLE] == ~0ull) {
        state.setRange(sim::DefaultConfig::DEFINED, internalOffsets[INT_ENABLE], 1);
        state.setRange(sim::DefaultConfig::VALUE, internalOffsets[INT_ENABLE], 1);
    } else
        state.copyRange(internalOffsets[INT_ENABLE], state, inputOffsets[ENABLE], 1);
}

void Node_Register::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
    HCL_ASSERT(clockPort == 0);
    
    bool enableDefined = state.get(sim::DefaultConfig::DEFINED, internalOffsets[INT_ENABLE]);
    bool enable = state.get(sim::DefaultConfig::VALUE, internalOffsets[INT_ENABLE]);
    
    if (!enableDefined) {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
    } else
        if (enable) 
            state.copyRange(outputOffsets[0], state, internalOffsets[INT_DATA], getOutputConnectionType(0).width);
}


std::string Node_Register::getTypeName() const
{ 
    return "Register"; 
}

void Node_Register::assertValidity() const 
{ 
    
}

std::string Node_Register::getInputName(size_t idx) const 
{ 
    switch (idx) { 
        case DATA: return "data_in"; 
        case RESET_VALUE: return "reset_value"; 
        case ENABLE: return "enable"; 
        default: 
            return "INVALID"; 
    } 
}

std::string Node_Register::getOutputName(size_t idx) const 
{ 
    return "data_out";
}

std::vector<size_t> Node_Register::getInternalStateSizes() const 
{
    std::vector<size_t> res(NUM_INTERNALS);
    res[INT_DATA] = getOutputConnectionType(0).width;
    res[INT_ENABLE] = 1;
    return res;
}


}
