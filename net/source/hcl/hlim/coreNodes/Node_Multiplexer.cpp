#include "Node_Multiplexer.h"


namespace hcl::core::hlim {

void Node_Multiplexer::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(1+operand, port); 
    setOutputConnectionType(0, port.node->getOutputConnectionType(port.port)); 
}

void Node_Multiplexer::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    auto selectorDriver = getNonSignalDriver(0);
    if (selectorDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    
    const auto &selectorType = selectorDriver.node->getOutputConnectionType(selectorDriver.port);
    HCL_ASSERT_HINT(selectorType.width <= 64, "Multiplexer with more than 64 bit selector not possible!");
    
    if (!allDefinedNonStraddling(state, inputOffsets[0], selectorType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
   
    std::uint64_t selector = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], selectorType.width);
    
    if (selector >= getNumInputPorts()-1) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    if (inputOffsets[1+selector] != ~0ull)
        state.copyRange(outputOffsets[0], state, inputOffsets[1+selector], getOutputConnectionType(0).width);
    else
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}

}
