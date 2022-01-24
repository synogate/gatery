/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include "Node_Arithmetic.h"

#include "../../utils/BitManipulation.h"
#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

#include "../SignalDelay.h"

#include <boost/multiprecision/cpp_int.hpp>

namespace gtry::hlim {

Node_Arithmetic::Node_Arithmetic(Op op) : Node(2, 1), m_op(op)
{

}

void Node_Arithmetic::connectInput(size_t operand, const NodePort &port)
{
    NodeIO::connectInput(operand, port);
    updateConnectionType();
}

void Node_Arithmetic::updateConnectionType()
{
    auto lhs = getDriver(0);
    auto rhs = getDriver(1);

    ConnectionType desiredConnectionType = getOutputConnectionType(0);

    if (lhs.node != nullptr) {
        desiredConnectionType = hlim::getOutputConnectionType(lhs);
        if (rhs.node != nullptr) {
            desiredConnectionType.width = std::max(desiredConnectionType.width, getOutputWidth(rhs));
            HCL_ASSERT_HINT(lhs.node->getOutputConnectionType(lhs.port).interpretation == rhs.node->getOutputConnectionType(rhs.port).interpretation, "Mixing different interpretations not yet implemented!");
        }
    } else if (rhs.node != nullptr)
        desiredConnectionType = hlim::getOutputConnectionType(rhs);

    setOutputConnectionType(0, desiredConnectionType);
}


void Node_Arithmetic::disconnectInput(size_t operand)
{
    NodeIO::disconnectInput(operand);
}


void Node_Arithmetic::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
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

    if (!allDefinedNonStraddling(state, inputOffsets[0], leftType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    if (!allDefinedNonStraddling(state, inputOffsets[1], rightType.width)) {
        state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
        return;
    }

    if (getOutputConnectionType(0).width <= 64 && leftType.width <= 64 && rightType.width <= 64) {
        std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
        std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);
        std::uint64_t result;

        switch (getOutputConnectionType(0).interpretation) {
            case ConnectionType::BOOL:
                HCL_ASSERT_HINT(false, "Can't do arithmetic on booleans!");
            break;
            case ConnectionType::BITVEC:
                switch (m_op) {
                    case ADD:
                        result = left + right;
                    break;
                    case SUB:
                        result = left - right;
                    break;
                    case MUL:
                        result = left * right;
                    break;
                    case DIV:
                        result = left / right;
                    break;
                    case REM:
                        result = left % right;
                    break;
                    default:
                        HCL_ASSERT_HINT(false, "Unhandled case!");
                }
            break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled case!");
        }

        state.insertNonStraddling(sim::DefaultConfig::VALUE, outputOffsets[0], getOutputConnectionType(0).width, result);
        state.insertNonStraddling(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, ~0ull);
    } else {
    /*
        typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<64, 0, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>> BigInt;

        BigInt left, right, result;


        boost::multiprecision::import_bits(
            left, 
            state.data(DefaultConfig::VALUE), 
            state.data(DefaultConfig::VALUE) + state.getNumBlocks(),
            DefaultConfig::NUM_BITS_PER_BLOCK,
            false
        );
        std::uint64_t left = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], leftType.width);
        std::uint64_t right = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[1], rightType.width);



        switch (getOutputConnectionType(0).interpretation) {
            case ConnectionType::BOOL:
                HCL_ASSERT_HINT(false, "Can't do arithmetic on booleans!");
            break;
            case ConnectionType::BITVEC:
                switch (m_op) {
                    case ADD:
                        result = left + right;
                    break;
                    case SUB:
                        result = left - right;
                    break;
                    case MUL:
                        result = left * right;
                    break;
                    case DIV:
                        result = left / right;
                    break;
                    case REM:
                        result = left % right;
                    break;
                    default:
                        HCL_ASSERT_HINT(false, "Unhandled case!");
                }
            break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled case!");
        }

*/
    }
}


std::string Node_Arithmetic::getTypeName() const
{
    switch (m_op) {
        case ADD: return "add";
        case SUB: return "sub";
        case MUL: return "mul";
        case DIV: return "div";
        case REM: return "remainder";
        default: return "Arithmetic";
    }
}

void Node_Arithmetic::assertValidity() const
{

}

std::string Node_Arithmetic::getInputName(size_t idx) const
{
    return idx==0?"a":"b";
}

std::string Node_Arithmetic::getOutputName(size_t idx) const
{
    return "out";
}

std::unique_ptr<BaseNode> Node_Arithmetic::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Arithmetic(m_op));
    copyBaseToClone(res.get());
    return res;
}

std::string Node_Arithmetic::attemptInferOutputName(size_t outputPort) const
{
    std::stringstream name;

    auto driver0 = getDriver(0);
    if (driver0.node == nullptr) return "";
    if (driver0.node->getName().empty()) return "";

    name << driver0.node->getName();

    switch (m_op) {
        case ADD: name << "_plus_"; break;
        case SUB: name << "_minus_"; break;
        case MUL: name << "_times_"; break;
        case DIV: name << "_over_"; break;
        case REM: name << "_rem_"; break;
        default: name << "_arith_op_"; break;
    }

    auto driver1 = getDriver(1);
    if (driver1.node == nullptr) return "";
    if (driver1.node->getName().empty()) return "";

    name << driver1.node->getName();

    return name.str();
}


void Node_Arithmetic::estimateSignalDelay(SignalDelay &sigDelay)
{
    auto inDelay0 = sigDelay.getDelay(getDriver(0));
    auto inDelay1 = sigDelay.getDelay(getDriver(1));

    HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
    auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

    auto width = getOutputConnectionType(0).width;


    switch (m_op) {
        case ADD:
        case SUB: {
            float routing = width * 0.8f;
            float compute = width * 0.2f;

            // very rough for now
            float maxDelay = 0.0f;
            for (auto i : utils::Range(width)) {
                maxDelay = std::max(maxDelay, inDelay0[i]);
                maxDelay = std::max(maxDelay, inDelay1[i]);
            }
            for (auto i : utils::Range(width)) {
                outDelay[i] = maxDelay + routing + compute;
            }
        } break;
        case MUL:
        case DIV:
        case REM:
        default:
            HCL_ASSERT_HINT(false, "Unhandled arithmetic operation for timing analysis!");
        break;
    }
}

void Node_Arithmetic::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
    auto inDelay0 = sigDelay.getDelay(getDriver(0));
    auto inDelay1 = sigDelay.getDelay(getDriver(1));

    auto width = getOutputConnectionType(0).width;

    float maxDelay = 0.0f;
    size_t maxIP = 0;
    size_t maxIB = 0;

    for (auto i : utils::Range(width))
        if (inDelay0[i] > maxDelay) {
            maxDelay = inDelay0[i];
            maxIP = 0;
            maxIB = i;
        }

    for (auto i : utils::Range(width))
        if (inDelay1[i] > maxDelay) {
            maxDelay = inDelay1[i];
            maxIP = 1;
            maxIB = i;
        }

    inputPort = maxIP;
    inputBit = maxIB;
}


}
