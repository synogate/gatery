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
#include "hcl/pch.h"
#include "Node_Pin.h"

namespace hcl::hlim {


Node_Pin::Node_Pin() : Node(1, 1)
{
    setOutputType(0, OUTPUT_IMMEDIATE);
}

void Node_Pin::setBool()
{
    setOutputConnectionType(0, {.interpretation = ConnectionType::BOOL, .width = 1});
}

void Node_Pin::setWidth(size_t width)
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
    if (m_differential)
        return "ioPin_differential";
    else
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

void Node_Pin::setDifferential(std::string_view posPrefix, std::string_view negPrefix)
{
    m_differential = true;
    m_differentialPosName = m_name + std::string(posPrefix);
    m_differentialNegName = m_name + std::string(negPrefix);
}


}
