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

#include "ConstructionTimeSimulationContext.h"

#include "ReferenceSimulator.h"
#include "BitAllocator.h"
#include "../hlim/Circuit.h"
#include "../hlim/Node.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Pin.h"

#include "../export/DotExport.h"

namespace gtry::sim {

void ConstructionTimeSimulationContext::overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state)
{
    m_overrides[output] = state;
}

void ConstructionTimeSimulationContext::getSignal(hlim::NodePort output, DefaultBitVectorState &state)
{
    // Basic idea: Find and copy the combinatorial subnet. Then optimize and execute the subnet to find the value.
    hlim::Circuit simCircuit;

    std::vector<hlim::NodePort> inputPorts;
    std::vector<hlim::NodePort> outputPorts = {output};

    std::map<hlim::NodePort, hlim::NodePort> outputsTranslated;
    std::map<hlim::NodePort, hlim::NodePort> outputsShorted;
    std::set<hlim::NodePort> outputsHandled;
    std::vector<hlim::NodePort> openList;
    openList.push_back(output);

    // Find all inputs/limits to combinatorial subnets, create constant nodes for those inputs
    while (!openList.empty()) {
        auto nodePort = openList.back();
        openList.pop_back();

        if (outputsHandled.contains(nodePort)) continue;
        outputsHandled.insert(nodePort);

        // check overrides
        {
            auto it = m_overrides.find(nodePort);
            if (it != m_overrides.end()) {
                auto type = hlim::getOutputConnectionType(nodePort);
                HCL_ASSERT(type.width == it->second.size());
                auto *c_node = simCircuit.createNode<hlim::Node_Constant>(it->second, type.interpretation);
                outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

                for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                    inputPorts.push_back(c);

                continue;
            }
        }

        // try use reset value
        if (auto *reg = dynamic_cast<hlim::Node_Register*>(nodePort.node)) {
            auto type = hlim::getOutputConnectionType(nodePort);

            auto reset = reg->getNonSignalDriver(hlim::Node_Register::Input::RESET_VALUE);
            if (reset.node != nullptr) {
                outputPorts.push_back(reset);
                openList.push_back(reset);

                for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                    outputsShorted[c] = reset;
            } else {
                DefaultBitVectorState undefinedState;
                undefinedState.resize(type.width);
                undefinedState.clearRange(DefaultConfig::DEFINED, 0, type.width);

                auto *c_node = simCircuit.createNode<hlim::Node_Constant>(std::move(undefinedState), type.interpretation);
                outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};
            }

            for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                inputPorts.push_back(c);

            continue;
        }

        // use undefined for everything non-combinatorial
        if (!nodePort.node->isCombinatorial()) {
            auto type = hlim::getOutputConnectionType(nodePort);

            DefaultBitVectorState undefinedState;
            undefinedState.resize(type.width);
            undefinedState.clearRange(DefaultConfig::DEFINED, 0, type.width);

            auto *c_node = simCircuit.createNode<hlim::Node_Constant>(std::move(undefinedState), type.interpretation);
            outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

            for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
                inputPorts.push_back(c);

            continue;
        }

        // explore inputs
        for (auto i : utils::Range(nodePort.node->getNumInputPorts())) {
            auto driver = nodePort.node->getDriver(i);
            if (driver.node != nullptr)
                openList.push_back(driver);
        }
    }

    //visualize(simCircuit, "/tmp/circuit_01");

    // Copy subnet
    std::map<hlim::BaseNode*, hlim::BaseNode*> mapSrc2Dst;
    simCircuit.copySubnet(inputPorts, outputPorts, mapSrc2Dst);

    //visualize(simCircuit, "/tmp/circuit_02");

    // Link to const nodes or short
    for (auto np : inputPorts) {
        // only care about input ports to nodes that are part of the new subnet
        auto it = mapSrc2Dst.find(np.node);
        if (it != mapSrc2Dst.end()) {
            auto *oldConsumer = it->first;
            auto *newConsumer = it->second;

            // Translate the driver of that input
            auto oldDriver = oldConsumer->getDriver(np.port);
            auto it2 = outputsTranslated.find(oldDriver);
            if (it2 != outputsTranslated.end()) { // its a link to a const node
                auto newDriver = it2->second;
                // Rewire the corresponding consumer in the new subnet
                newConsumer->rewireInput(np.port, newDriver);
            } else { // its shorted, e.g. to bypass a register
                // Find where it was supposed to be bypassed to in the old circuit
                auto it2 = outputsShorted.find(np); HCL_ASSERT(it2 != outputsShorted.end());
                // Find the corresponding producer in the new circuit
                auto it3 = mapSrc2Dst.find(it2->second.node); HCL_ASSERT(it3 != mapSrc2Dst.end());
                auto newProducer = it3->second;
                // Rewire
                newConsumer->rewireInput(np.port, {.node=newProducer, .port=it2->second.port});
            }
        }
    }

    //visualize(simCircuit, "/tmp/circuit_03");

    // Translate the output of interest
    hlim::NodePort newOutput = output;
    {
        auto it = outputsTranslated.find(output);
        if (it != outputsTranslated.end())
            newOutput = it->second;
        else
            newOutput.node = mapSrc2Dst.find(output.node)->second;
    }

    // Force output's existance throughout optimization
    auto *pin = simCircuit.createNode<hlim::Node_Pin>();
    pin->connect(newOutput);

    //visualize(simCircuit, "/tmp/circuit_04");

    // optimize
    simCircuit.optimize(3);

    // Reestablish output from pin
    newOutput = pin->getDriver(0);

    //visualize(simCircuit, "/tmp/circuit_05");

    // Run simulation
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileProgram(simCircuit);
    simulator.powerOn();
    simulator.reevaluate();

    // Fetch result
    state = simulator.getValueOfOutput(newOutput);
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor)
{
    HCL_ASSERT_HINT(false, "Simulation coroutine attemped to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil)
{
    HCL_ASSERT_HINT(false, "Simulation coroutine attemped to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock)
{
    HCL_ASSERT_HINT(false, "Simulation coroutine attemped to run (and suspend) outside of simulation!");
}

}
