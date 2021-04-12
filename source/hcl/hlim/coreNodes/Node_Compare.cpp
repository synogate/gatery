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
#include "Node_Compare.h"


namespace hcl::hlim {

Node_Compare::Node_Compare(Op op) : Node(2, 1), m_op(op)
{
    ConnectionType conType;
    conType.width = 1;
    conType.interpretation = ConnectionType::BOOL;
    setOutputConnectionType(0, conType);
}

void Node_Compare::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    auto leftDriver = getDriver(0);
    auto rightDriver = getDriver(1);
    if (inputOffsets[0] == ~0ull || inputOffsets[1] == ~0ull ||
        leftDriver.node== nullptr || rightDriver.node == nullptr) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    const auto &leftType = hlim::getOutputConnectionType(leftDriver);
    const auto &rightType = hlim::getOutputConnectionType(rightDriver);
    HCL_ASSERT_HINT(leftType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    HCL_ASSERT_HINT(rightType.width <= 64, "Compare with more than 64 bits not yet implemented!");
    HCL_ASSERT_HINT(leftType.interpretation == rightType.interpretation, "Comparing signals with different interpretations not yet implemented!");

    if (!allDefinedNonStraddling(state, inputOffsets[0], leftType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }
    if (!allDefinedNonStraddling(state, inputOffsets[1], rightType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
    std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);
    bool result;

    switch (leftType.interpretation) {
        case ConnectionType::BOOL:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        case ConnectionType::BITVEC:
            switch (m_op) {
                case EQ:
                    result = left == right;
                break;
                case NEQ:
                    result = left != right;
                break;
                case LT:
                    result = left < right;
                break;
                case GT:
                    result = left > right;
                break;
                case LEQ:
                    result = left <= right;
                break;
                case GEQ:
                    result = left >= right;
                break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled case!");
            }
        break;
        default:
            HCL_ASSERT_HINT(false, "Unhandled case!");
    }

    state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], 1, result?1:0);
    state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], 1, 1);
}


std::string Node_Compare::getTypeName() const
{
    switch (m_op) {
        case EQ:  return "==";
        case NEQ: return "!=";
        case LT:  return "<";
        case GT:  return ">";
        case LEQ: return "<=";
        case GEQ: return ">=";
        default:  return "Compare";
    }
}

void Node_Compare::assertValidity() const
{

}

std::string Node_Compare::getInputName(size_t idx) const
{
    return idx==0?"a":"b";
}

std::string Node_Compare::getOutputName(size_t idx) const
{
    return "out";
}

std::unique_ptr<BaseNode> Node_Compare::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Compare(m_op));
    copyBaseToClone(res.get());
    return res;
}

std::string Node_Compare::attemptInferOutputName(size_t outputPort) const
{
    std::stringstream name;

    auto driver0 = getDriver(0);
    if (driver0.node == nullptr) return "";
    if (driver0.node->getName().empty()) return "";

    name << driver0.node->getName();

    switch (m_op) {
        case EQ:  name << "_eq_"; break;
        case NEQ: name << "_neq_"; break;
        case LT:  name << "_lt_"; break;
        case GT:  name << "_gt_"; break;
        case LEQ: name << "_leq_"; break;
        case GEQ: name << "_geq_"; break;
        default:  name << "_cmp_"; break;
    }

    auto driver1 = getDriver(1);
    if (driver1.node == nullptr) return "";
    if (driver1.node->getName().empty()) return "";

    name << driver1.node->getName();

    return name.str();
}



}
