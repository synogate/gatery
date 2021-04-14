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
#include "Node_Logic.h"

namespace hcl::hlim {

Node_Logic::Node_Logic(Op op) : Node(op==NOT?1:2, 1), m_op(op)
{

}

void Node_Logic::connectInput(size_t operand, const NodePort &port)
{
    NodeIO::connectInput(operand, port);
    updateConnectionType();
}

void Node_Logic::disconnectInput(size_t operand)
{
    NodeIO::disconnectInput(operand);
}

void Node_Logic::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    size_t width = getOutputConnectionType(0).width;

    NodePort leftDriver = getDriver(0);
    bool leftAllUndefined = leftDriver.node == nullptr;
    /*
    if (leftDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    */

    NodePort rightDriver;
    bool rightAllUndefined = true;
    if (m_op != NOT) {
        rightDriver = getDriver(1);
        rightAllUndefined = rightDriver.node == nullptr;
    }

    size_t offset = 0;

    while (offset < width) {
        size_t chunkSize = std::min<size_t>(64, width-offset);

        std::uint64_t left, leftDefined, right, rightDefined;

        if (leftAllUndefined || inputOffsets[0] == ~0ull) {
            leftDefined = 0;
            left = 0;
        } else {
            leftDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[0]+offset, chunkSize);
            left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0]+offset, chunkSize);
        }

        if (rightAllUndefined || m_op == NOT || inputOffsets[1] == ~0ull) {
            rightDefined = 0;
            right = 0;
        } else {
            rightDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[1]+offset, chunkSize);
            right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1]+offset, chunkSize);
        }

        std::uint64_t result, resultDefined;
        switch (m_op) {
            case AND:
                result = left & right;
                resultDefined = (leftDefined & !left) | (rightDefined & !right) | (leftDefined & rightDefined);
            break;
            case NAND:
                result = ~(left & right);
                resultDefined = (leftDefined & !left) | (rightDefined & !right) | (leftDefined & rightDefined);
            break;
            case OR:
                result = left | right;
                resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined & rightDefined);
            break;
            case NOR:
                result = ~(left | right);
                resultDefined = (leftDefined & left) | (rightDefined & right) | (leftDefined  & rightDefined);
            break;
            case XOR:
                result = left ^ right;
                resultDefined = leftDefined & rightDefined;
            break;
            case EQ:
                result = ~(left ^ right);
                resultDefined = leftDefined & rightDefined;
            break;
            case NOT:
                result = ~left;
                resultDefined = leftDefined;
            break;
        };

        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0] + offset, chunkSize, result);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0] + offset, chunkSize, resultDefined);
        offset += chunkSize;
    }
}

std::string Node_Logic::getTypeName() const
{
    switch (m_op) {
        case AND: return "and";
        case NAND: return "nand";
        case OR: return "or";
        case NOR: return "nor";
        case XOR: return "xor";
        case EQ: return "bitwise-equal";
        case NOT: return "not";
        default:
            return "Logic";
    }
}

void Node_Logic::assertValidity() const
{

}

std::string Node_Logic::getInputName(size_t idx) const
{
    return idx==0?"a":"b";
}

std::string Node_Logic::getOutputName(size_t idx) const
{
    return "output";
}


void Node_Logic::updateConnectionType()
{
    auto lhs = getDriver(0);
    NodePort rhs;
    if (m_op != NOT)
        rhs = getDriver(1);

    ConnectionType desiredConnectionType = getOutputConnectionType(0);

    if (lhs.node != nullptr) {
        if (rhs.node != nullptr) {
            desiredConnectionType = hlim::getOutputConnectionType(lhs);
            HCL_ASSERT_HINT(desiredConnectionType == hlim::getOutputConnectionType(rhs), "Support for differing types of input to logic node not yet implemented");
            //desiredConnectionType.width = std::max(desiredConnectionType.width, rhs.node->getOutputConnectionType(rhs.port).width);
        } else
            desiredConnectionType = hlim::getOutputConnectionType(lhs);
    } else if (rhs.node != nullptr)
        desiredConnectionType = hlim::getOutputConnectionType(rhs);

    setOutputConnectionType(0, desiredConnectionType);
}


std::unique_ptr<BaseNode> Node_Logic::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Logic(m_op));
    copyBaseToClone(res.get());
    return res;
}

std::string Node_Logic::attemptInferOutputName(size_t outputPort) const
{
    std::stringstream name;

    auto driver0 = getDriver(0);
    if (driver0.node == nullptr) return "";
    if (driver0.node->getName().empty()) return "";

    name << driver0.node->getName();

    switch (m_op) {
        case AND: name << "_and_"; break;
        case NAND: name << "_nand_"; break;
        case OR: name << "_or_"; break;
        case NOR: name << "_nor_"; break;
        case XOR: name << "_xor_"; break;
        case EQ: name << "_bweq_"; break;
        case NOT: name << "_not"; break;
        default: name << "_logic_"; break;
    }

    if (getNumInputPorts() > 1) {
        auto driver1 = getDriver(1);
        if (driver1.node == nullptr) return "";
        if (driver1.node->getName().empty()) return "";

        name << driver1.node->getName();
    }
    return name.str();
}

}
