#include "Node_Register.h"
#include "Node_Constant.h"

namespace mhdl::core::hlim {

Node_Register::Node_Register() : Node(NUM_INPUTS, 1) 
{ 
    m_clocks.resize(1);
    setOutputType(0, OUTPUT_LATCHED);
}

void Node_Register::connectInput(Input input, const NodePort &port) 
{ 
    NodeIO::connectInput(input, port); 
    if (input == DATA || input == RESET_VALUE) 
        setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Register::setClock(BaseClock *clk)
{
    attachClock(clk, 0);
}

void Node_Register::setReset(std::string resetName)
{
    m_resetName = std::move(resetName);
}

void Node_Register::simulateReset(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
    auto resetDriver = getNonSignalDriver(RESET_VALUE);
    if (resetDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, internalOffsets[0], getOutputConnectionType(0).width, false);
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    Node_Constant *constNode = dynamic_cast<Node_Constant *>(resetDriver.node);
    MHDL_ASSERT_HINT(constNode != nullptr, "Constant value propagation is not yet implemented, so for simulation the register reset value must be connected to a constant node via signals only!");
    
    size_t width = getOutputConnectionType(0).width;
    size_t offset = 0;
    
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);
        
        std::uint64_t block = 0;
        for (auto i : utils::Range(chunkSize))
            if (constNode->getValue().bitVec[offset + i])
                block |= 1 << i;
                    
        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize, block);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize, ~0ull);    
        
        state.insertNonStraddling(sim::DefaultConfig::VALUE, internalOffsets[0] + offset, chunkSize, block);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, internalOffsets[0] + offset, chunkSize, ~0ull);    
        
        offset += chunkSize;
    }
    
}

void Node_Register::simulateEvaluate(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    state.copyRange(internalOffsets[0], state, inputOffsets[DATA], getOutputConnectionType(0).width);
}

void Node_Register::simulateAdvance(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets, size_t clockPort) const
{
    MHDL_ASSERT(clockPort == 0);
    
    auto enableDriver = getNonSignalDriver(ENABLE);
    if (enableDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    } 
    
    if (!allDefinedNonStraddling(state, inputOffsets[ENABLE], 1)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
   
    std::uint64_t enable = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[ENABLE], 1);
    
    if (!enable) 
        return;
    

    state.copyRange(outputOffsets[0], state, internalOffsets[0], getOutputConnectionType(0).width);
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
    return {getOutputConnectionType(0).width};
}


}
