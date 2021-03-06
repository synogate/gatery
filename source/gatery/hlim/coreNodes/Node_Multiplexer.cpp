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
#include "Node_Multiplexer.h"


namespace gtry::hlim {

void Node_Multiplexer::connectInput(size_t operand, const NodePort &port)
{
    NodeIO::connectInput(1+operand, port);
    if (port.node != nullptr)
        setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Multiplexer::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    auto selectorDriver = getDriver(0);
    if (inputOffsets[0] == ~0ull) {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
        return;
    }

    const auto &selectorType = selectorDriver.node->getOutputConnectionType(selectorDriver.port);
    HCL_ASSERT_HINT(selectorType.width <= 64, "Multiplexer with more than 64 bit selector not possible!");

    if (!allDefinedNonStraddling(state, inputOffsets[0], selectorType.width)) {

#if 0
        // Check if all inputs equal (should be more fine grained based on the individual bits!)
        bool allInputsEqual = true;

        // All must be equal to first in which bits are defined and in the defined bits
        for (unsigned i = 2; i < getNumInputPorts(); i++)
            allInputsEqual &= equalOnDefinedValues(state, inputOffsets[1], state, inputOffsets[i], getOutputConnectionType(0).width);

        if (allInputsEqual && getNumInputPorts() > 1) {
            // arbitrarily copy first
            state.copyRange(outputOffsets[0], state, inputOffsets[1], getOutputConnectionType(0).width);
        } else
            state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
#else
        ///@todo: This is probably slow

        for (auto b : utils::Range(getOutputConnectionType(0).width)) {
            bool value = state.get(sim::DefaultConfig::VALUE, inputOffsets[1]+b);
            bool defined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[1]+b);

            // while still defined, check all corresponding bits in other inputs
            // if defined and same value, remain defined
            if (defined)
                for (unsigned i = 2; i < getNumInputPorts(); i++) {
                    bool v = state.get(sim::DefaultConfig::VALUE, inputOffsets[i]+b);
                    bool d = state.get(sim::DefaultConfig::DEFINED, inputOffsets[i]+b);

                    if (!d || value != v) {
                        defined = false;
                        break;
                    }
                }

            state.set(sim::DefaultConfig::VALUE, outputOffsets[0]+b, value);
            state.set(sim::DefaultConfig::DEFINED, outputOffsets[0]+b, defined);
        }
#endif

        return;
    }

    std::uint64_t selector = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[0], selectorType.width);

    if (selector >= getNumInputPorts()-1) {
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
        return;
    }
    if (inputOffsets[1+selector] != ~0ull)
        state.copyRange(outputOffsets[0], state, inputOffsets[1+selector], getOutputConnectionType(0).width);
    else
        state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}



std::unique_ptr<BaseNode> Node_Multiplexer::cloneUnconnected() const
{
    std::unique_ptr<BaseNode> res(new Node_Multiplexer(getNumInputPorts()-1));
    copyBaseToClone(res.get());
    return res;
}


std::string Node_Multiplexer::attemptInferOutputName(size_t outputPort) const
{
#if 1
    std::string longestInput;
    for (auto i : utils::Range((size_t) 1, getNumInputPorts())) {
        auto driver = getDriver(i);
        if (driver.node == nullptr)
            continue;
        if (driver.node->getOutputConnectionType(driver.port).interpretation == ConnectionType::DEPENDENCY) continue;
        if (driver.node->getName().empty())
            continue;

        if (driver.node->getName().length() > longestInput.length())
            longestInput = driver.node->getName();
    }

    std::stringstream name;
    name << longestInput << "_mux";
    return name.str();
#else
    std::stringstream name;
    bool first = true;
    for (auto i : utils::Range((size_t) 1, getNumInputPorts())) {
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
    name << "_mux";
    return name.str();
#endif
}



}
