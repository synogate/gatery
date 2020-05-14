#include "Node_Rewire.h"

namespace mhdl::core::hlim {

bool Node_Rewire::RewireOperation::isBitExtract(size_t& bitIndex) const
{
    if (ranges.size() == 1) {
        if (ranges[0].subwidth == 1 &&
            ranges[0].source == OutputRange::INPUT &&
            ranges[0].inputIdx == 0) {
            
            bitIndex = ranges[0].inputOffset;
            return true;
        }
    }
    return false;
}

Node_Rewire::Node_Rewire(size_t numInputs) : Node(numInputs, 1)
{
    
}

void Node_Rewire::connectInput(size_t operand, const NodePort &port) 
{ 
    NodeIO::connectInput(operand, port); 
    updateConnectionType();
}


void Node_Rewire::updateConnectionType()
{
    auto lhs = getDriver(0);
    
    ConnectionType desiredConnectionType = getOutputConnectionType(0);
    
    if (lhs.node != nullptr)
        desiredConnectionType = lhs.node->getOutputConnectionType(lhs.port);
    
    desiredConnectionType.width = 0;
    for (auto r : m_rewireOperation.ranges)
        desiredConnectionType.width += r.subwidth;

    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Rewire::simulateEvaluate(sim::DefaultBitVectorState &state, const size_t internalOffset, const size_t *inputOffsets, const size_t *outputOffsets) const 
{
    MHDL_ASSERT_HINT(getOutputConnectionType(0).width <= 64, "Rewiring with more than 64 bits not yet implemented!");

    size_t outputOffset = 0;
    for (const auto &range : m_rewireOperation.ranges) {
        if (range.source == OutputRange::INPUT) {
            auto driver = getNonSignalDriver(range.inputIdx);
            if (driver.node == nullptr) continue;
    
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth,
                    state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth));
            
            state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + outputOffset, range.subwidth,
                    state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth));
        } else {
            std::uint64_t output = range.source == OutputRange::CONST_ZERO?0ull:~0ull;
            state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth, ~0ull);
            state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + outputOffset, range.subwidth, output);
        }
        outputOffset += range.subwidth;
    }
}

std::string Node_Rewire::getTypeName() const 
{ 
    size_t bitIndex;
    if (m_rewireOperation.isBitExtract(bitIndex))
        return std::string("bit ") + std::to_string(bitIndex);
    else
        return "Rewire"; 
}



}
