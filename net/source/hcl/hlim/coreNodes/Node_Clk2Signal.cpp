#include "Node_Clk2Signal.h"

namespace hcl::core::hlim {

Node_Clk2Signal::Node_Clk2Signal() : Node(0, 1)
{
    ConnectionType conType;
    conType.width = 1;
    conType.interpretation = ConnectionType::BOOL;
    setOutputConnectionType(0, conType);

    m_clocks.resize(1);
    setOutputType(0, OUTPUT_LATCHED);    
}

std::string Node_Clk2Signal::getTypeName() const
{
    return "clk2signal";
}

void Node_Clk2Signal::assertValidity() const
{
}

std::string Node_Clk2Signal::getInputName(size_t idx) const
{
    return "";
}

std::string Node_Clk2Signal::getOutputName(size_t idx) const
{
    return "clk";
}

void Node_Clk2Signal::setClock(Clock *clk)
{
    attachClock(clk, 0);
}

}
