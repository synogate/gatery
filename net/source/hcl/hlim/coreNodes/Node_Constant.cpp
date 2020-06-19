#include "Node_Constant.h"

#include <sstream>

namespace mhdl::core::hlim {

Node_Constant::Node_Constant(ConstantData value, const hlim::ConnectionType& connectionType) : Node(0, 1), m_Value(value) 
{
    setOutputConnectionType(0, connectionType); 
    setOutputType(0, OUTPUT_CONSTANT);
}

void Node_Constant::simulateReset(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const 
{
    size_t width = getOutputConnectionType(0).width;
    size_t offset = 0;
    
    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);
        
        std::uint64_t block = 0;
        for (auto i : utils::Range(chunkSize))
            if (m_Value.bitVec[offset + i])
                block |= 1ull << i;
                    
        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize, block);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize, ~0ull);    
        
        offset += chunkSize;
    }
}

std::string Node_Constant::getTypeName() const 
{ 
    std::stringstream bitStream;
    ///@todo respect stored base
    for (auto b : m_Value.bitVec)
        if (b)
            bitStream << "1";
        else
            bitStream << "0";
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
