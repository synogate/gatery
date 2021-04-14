/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "Node_Clk2Signal.h"

namespace hcl::hlim {

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
