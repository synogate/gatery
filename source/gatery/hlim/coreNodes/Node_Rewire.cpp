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
#include "Node_Rewire.h"

namespace gtry::hlim {

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

Node_Rewire::RewireOperation& Node_Rewire::RewireOperation::addInput(size_t inputIndex, size_t inputOffset, size_t width)
{
    if (width > 0)
    {
        ranges.emplace_back(OutputRange{
            .subwidth = width,
            .source = OutputRange::INPUT,
            .inputIdx = inputIndex,
            .inputOffset = inputOffset
            });
    }
    return *this;
}

Node_Rewire::RewireOperation& Node_Rewire::RewireOperation::addConstant(OutputRange::Source type, size_t width)
{
    HCL_ASSERT(type != OutputRange::INPUT);

    if (width > 0)
    {
        ranges.emplace_back(OutputRange{
            .subwidth = width,
            .source = type,
            .inputIdx = 0,
            .inputOffset = 0
            });
    }
    return *this;
}

Node_Rewire::Node_Rewire(size_t numInputs) : Node(numInputs, 1)
{
}

void Node_Rewire::connectInput(size_t operand, const NodePort &port)
{
    NodeIO::connectInput(operand, port);
    updateConnectionType();
}

void Node_Rewire::setConcat()
{
    RewireOperation op;
    op.ranges.reserve(getNumInputPorts());

    for (size_t i = 0; i < getNumInputPorts(); ++i)
    {
        NodePort driver = getDriver(i);
        HCL_ASSERT(driver.node);
        size_t width = driver.node->getOutputConnectionType(driver.port).width;

        op.ranges.emplace_back(OutputRange{
            .subwidth = width,
            .source = OutputRange::INPUT,
            .inputIdx = i,
            .inputOffset = 0
        });
    }
    setOp(std::move(op));
}

void Node_Rewire::setExtract(size_t offset, size_t count)
{
    HCL_DESIGNCHECK(getNumInputPorts() == 1);

    RewireOperation op;
    op.addInput(0, offset, count);
    setOp(std::move(op));
}

void Node_Rewire::setReplaceRange(size_t offset)
{
    HCL_ASSERT(getNumInputPorts() == 2);

    const ConnectionType type0 = getDriverConnType(0);
    const ConnectionType type1 = getDriverConnType(1);
    HCL_ASSERT(type0.width >= type1.width + offset);

    hlim::Node_Rewire::RewireOperation op;
    op.addInput(0, 0, offset);
    op.addInput(1, 0, type1.width);
    op.addInput(0, offset + type1.width, type0.width - (offset + type1.width));

    setOp(std::move(op));
}

void Node_Rewire::setPadTo(size_t width, OutputRange::Source padding)
{
    HCL_ASSERT(getNumInputPorts() == 1);

    const ConnectionType type0 = getDriverConnType(0);

    hlim::Node_Rewire::RewireOperation op;
    op.addInput(0, 0, std::min(width, type0.width));
    if(width > type0.width)
        op.addConstant(padding, width - type0.width);

    setOp(std::move(op));
}

void Node_Rewire::setPadTo(size_t width)
{
    HCL_ASSERT(getNumInputPorts() == 1);

    const ConnectionType type0 = getDriverConnType(0);
    HCL_DESIGNCHECK(type0.width > 0);

    hlim::Node_Rewire::RewireOperation op;
    op.addInput(0, 0, std::min(width, type0.width));
    for(size_t i = type0.width; i < width; ++i)
        op.addInput(0, type0.width-1, 1);

    setOp(std::move(op));
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
    //HCL_ASSERT(desiredConnectionType.width <= 64);

    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Rewire::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    //HCL_ASSERT_HINT(getOutputConnectionType(0).width <= 64, "Rewiring with more than 64 bits not yet implemented!");

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

bool Node_Rewire::isNoOp() const
{
    if (getNumInputPorts() == 0) return false;
    if (getDriver(0).node == nullptr) return false;
    if (getOutputConnectionType(0) != getDriverConnType(0)) return false;

    auto outWidth = getOutputConnectionType(0).width;

    size_t offset = 0;
    for (const auto &range : m_rewireOperation.ranges) {
        if (range.source != OutputRange::INPUT) return false;
        if (range.inputIdx != 0) return false;
        if (range.inputOffset != offset) return false;
        offset += range.subwidth;
    }
    HCL_ASSERT(offset == outWidth);

    if (offset != getDriverConnType(0).width) return false;

    return true;
}




std::unique_ptr<BaseNode> Node_Rewire::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Rewire(getNumInputPorts()));
    copyBaseToClone(res.get());
    ((Node_Rewire*)res.get())->m_desiredConnectionType = m_desiredConnectionType;
    ((Node_Rewire*)res.get())->m_rewireOperation = m_rewireOperation;
    return res;
}


std::string Node_Rewire::attemptInferOutputName(size_t outputPort) const
{
    size_t bitIndex;
    if (m_rewireOperation.isBitExtract(bitIndex)) {
        auto driver0 = getDriver(0);
        if (driver0.node == nullptr) return "";
        if (driver0.node->getName().empty()) return "";

        std::stringstream name;
        name << driver0.node->getName() << "_bit_" << bitIndex;
        return name.str();
    } else {
        std::stringstream name;
        bool first = true;
        for (auto i : utils::Range(getNumInputPorts())) {
            auto driver = getDriver(i);
            if (driver.node == nullptr)
                return "";
            if (driver.node->getOutputConnectionType(driver.port).interpretation == ConnectionType::DEPENDENCY) continue;
            if (driver.node->getName().empty()) {
                return "";
            } else {
                if (!first) name << '_';
                first = false;
                name << driver.node->getName();
            }
        }
        name << "_rewired";
        return name.str();
    }
}




}
