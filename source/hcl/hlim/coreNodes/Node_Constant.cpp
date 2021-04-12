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
#include "Node_Constant.h"

#include <sstream>
#include <iomanip>

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
    bitStream << std::hex << m_Value;
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


std::unique_ptr<BaseNode> Node_Constant::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Constant(m_Value, getOutputConnectionType(0).interpretation));
    copyBaseToClone(res.get());
    return res;
}

std::string Node_Constant::attemptInferOutputName(size_t outputPort) const
{
    std::stringstream bitStream;
    bitStream << "const_" << std::hex << m_Value;
    return bitStream.str();
}


}
