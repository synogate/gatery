#include "Node_Multiplexer.h"


namespace mhdl::core::hlim {

void Node_Multiplexer::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(1+operand, port); 
    setOutputConnectionType(0, port.node->getOutputConnectionType(port.port)); 
}

void Node_Multiplexer::simulateEvaluate(sim::DefaultBitVectorState &state, const size_t *inputOffsets, const size_t *outputOffsets) const 
{
    auto selectorDriver = getNonSignalDriver(0);
    if (selectorDriver.node == nullptr) return;
    
    const auto &selectorType = selectorDriver.node->getOutputConnectionType(selectorDriver.port);
    MHDL_ASSERT_HINT(selectorType.width <= 64, "Multiplexer with more than 64 bit selector not possible!");
    
    if (!allDefinedNonStraddling(state, inputOffsets[0], selectorType.width)) return;
   
    std::uint64_t selector = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], selectorType.width);
    
    if (selector >= getNumInputPorts()-1) 
        return;
    
    size_t width = getOutputConnectionType(0).width;
    size_t offset = 0;
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);
        
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize,
                state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[1+selector]+offset, chunkSize));
        
        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize,
                state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1+selector]+offset, chunkSize));

        offset += chunkSize;
    }
}

}
