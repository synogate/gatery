#include "Node_Constant.h"

#include <sstream>

namespace hcl::core::hlim {

Node_Constant::Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType::Interpretation connectionType) :
    Node(0, 1), 
    m_Value(std::move(value))
{
    setOutputConnectionType(0, ConnectionType{ .interpretation = connectionType, .width = m_Value.size() });
    setOutputType(0, OUTPUT_CONSTANT);
}

void Node_Constant::simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const 
{
    state.insert(m_Value, outputOffsets[0]);
}

std::string Node_Constant::getTypeName() const 
{ 
    std::stringstream bitStream;
    bitStream << m_Value;
    return bitStream.str(); 
}

void Node_Constant::assertValidity() const 
{ 
    
}

std::string Node_Constant::getInputName(size_t idx) const 
{ 
    return ""; 
}

std::string Node_Constant::getOutputName(size_t idx) const 
{ 
    return "output"; 
}

}
