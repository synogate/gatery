#include "Node_Pin.h"

namespace hcl::core::hlim {


Node_Pin::Node_Pin() : Node(1, 1) 
{
    setOutputType(0, OUTPUT_LATCHED);
}

void Node_Pin::setBool()
{
    setOutputConnectionType(0, {.interpretation = ConnectionType::BOOL, .width = 1});
}

void Node_Pin::setWidth(unsigned width)
{
    setOutputConnectionType(0, {.interpretation = ConnectionType::BITVEC, .width = width});
}

void Node_Pin::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{

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

}