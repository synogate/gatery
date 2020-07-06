#include "Node_Rewire.h"

namespace hcl::core::hlim {

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

void Node_Rewire::changeOutputType(ConnectionType outputType)
{
    m_desiredConnectionType = outputType;
    updateConnectionType();
}

void Node_Rewire::updateConnectionType()
{
    ConnectionType desiredConnectionType = m_desiredConnectionType;
    
    desiredConnectionType.width = 0;
    for (auto r : m_rewireOperation.ranges)
        desiredConnectionType.width += r.subwidth;

    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Rewire::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    HCL_ASSERT_HINT(getOutputConnectionType(0).width <= 64, "Rewiring with more than 64 bits not yet implemented!");

    size_t outputOffset = 0;
    for (const auto &range : m_rewireOperation.ranges) {
        if (range.source == OutputRange::INPUT) {
            auto driver = getNonSignalDriver(range.inputIdx);
            if (driver.node == nullptr)
                state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
            else
                state.copyRange(outputOffsets[0] + outputOffset, state, inputOffsets[range.inputIdx]+range.inputOffset, range.subwidth);

        } else {
            state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0] + outputOffset, range.subwidth);
            state.setRange(sim::DefaultConfig::VALUE, outputOffsets[0] + outputOffset, range.subwidth, range.source == OutputRange::CONST_ONE);
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
