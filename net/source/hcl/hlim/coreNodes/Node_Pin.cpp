#include "Node_Pin.h"

namespace hcl::core::hlim {


Node_Pin::Node_Pin() : Node(1, 1)
{
    setOutputType(0, OUTPUT_IMMEDIATE);
}

void Node_Pin::setBool()
{
    setOutputConnectionType(0, {.interpretation = ConnectionType::BOOL, .width = 1});
}

void Node_Pin::setWidth(unsigned width)
{
    setOutputConnectionType(0, {.interpretation = ConnectionType::BITVEC, .width = width});
}

bool Node_Pin::isOutputPin() const
{
    return getDriver(0).node != nullptr;
}

std::vector<size_t> Node_Pin::getInternalStateSizes() const
{
    if (getDirectlyDriven(0).empty()) return {};

    return {getOutputConnectionType(0).width};
}

void Node_Pin::setState(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const sim::DefaultBitVectorState &newState)
{
    HCL_ASSERT(!getDirectlyDriven(0).empty());
    HCL_ASSERT(newState.size() == getOutputConnectionType(0).width);
    state.copyRange(internalOffsets[0], newState, 0, newState.size());
}

void Node_Pin::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    if (!getDirectlyDriven(0).empty())
        state.copyRange(outputOffsets[0], state, internalOffsets[0], getOutputConnectionType(0).width);
}

std::string Node_Pin::getTypeName() const
{
    return "ioPin";
}

void Node_Pin::assertValidity() const
{

}

std::string Node_Pin::getInputName(size_t idx) const
{
    return "in";
}

std::string Node_Pin::getOutputName(size_t idx) const
{
    return "out";
}

std::unique_ptr<BaseNode> Node_Pin::cloneUnconnected() const {
    std::unique_ptr<BaseNode> copy(new Node_Pin());
    copyBaseToClone(copy.get());

    return copy;
}

std::string Node_Pin::attemptInferOutputName(size_t outputPort) const
{
    return m_name;
}


}
