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
#include "SigHandle.h"

#include "../hlim/Circuit.h"
#include "../hlim/Node.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Pin.h"

#include "../export/DotExport.h"

namespace gtry::sim {

void ConstructionTimeSimulationContext::overrideSignal(const SigHandle &handle, const ExtendedBitVectorState &state)
{
	auto converted = sim::tryConvertToDefault(state);
	HCL_DESIGNCHECK_HINT(converted, "dont_care or high_impedance not supported in overrides for construction time simulation");

	m_overrides[handle.getOutput()] = *converted;
}

void ConstructionTimeSimulationContext::overrideRegister(const SigHandle &handle, const DefaultBitVectorState &state)
{
	m_overrides[handle.getOutput()] = state;
}

void ConstructionTimeSimulationContext::getSignal(const SigHandle &handle, DefaultBitVectorState &state)
{
	// Basic idea: Find and copy the combinatorial subnet. Then optimize and execute the subnet to find the value.
	hlim::Circuit simCircuit;

	utils::StableSet<hlim::NodePort> inputPorts;
	utils::StableSet<hlim::NodePort> outputPorts = {handle.getOutput()};

	utils::UnstableMap<hlim::NodePort, hlim::NodePort> outputsTranslated;
	utils::UnstableMap<hlim::NodePort, hlim::NodePort> outputsShorted;
	utils::UnstableSet<hlim::NodePort> outputsHandled;
	std::vector<hlim::NodePort> openList;
	openList.push_back(handle.getOutput());

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
				auto *c_node = simCircuit.createNode<hlim::Node_Constant>(it->second, type.type);
				c_node->recordStackTrace();
				c_node->moveToGroup(simCircuit.getRootNodeGroup());

				outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

				for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
					inputPorts.insert(c);

				continue;
			}
		}

		// try use reset value
		if (auto *reg = dynamic_cast<hlim::Node_Register*>(nodePort.node)) {
			auto type = hlim::getOutputConnectionType(nodePort);

			auto reset = reg->getNonSignalDriver(hlim::Node_Register::Input::RESET_VALUE);
			if (reset.node != nullptr) {
				outputPorts.insert(reset);
				openList.push_back(reset);

				for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
					outputsShorted[c] = reset;
			} else {
				DefaultBitVectorState undefinedState;
				undefinedState.resize(type.width);
				undefinedState.clearRange(DefaultConfig::DEFINED, 0, type.width);

				auto *c_node = simCircuit.createNode<hlim::Node_Constant>(std::move(undefinedState), type.type);
				c_node->recordStackTrace();
				c_node->moveToGroup(simCircuit.getRootNodeGroup());
				outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};
			}

			for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
				inputPorts.insert(c);

			continue;
		}

		// use undefined for everything non-combinatorial
		if (!nodePort.node->isCombinatorial(nodePort.port)) {
			auto type = hlim::getOutputConnectionType(nodePort);

			DefaultBitVectorState undefinedState;
			undefinedState.resize(type.width);
			undefinedState.clearRange(DefaultConfig::DEFINED, 0, type.width);

			auto *c_node = simCircuit.createNode<hlim::Node_Constant>(std::move(undefinedState), type.type);
			c_node->recordStackTrace();
			c_node->moveToGroup(simCircuit.getRootNodeGroup());
			outputsTranslated[nodePort] = {.node = c_node, .port = 0ull};

			for (auto c : nodePort.node->getDirectlyDriven(nodePort.port))
				inputPorts.insert(c);

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
	utils::StableMap<hlim::BaseNode*, hlim::BaseNode*> mapSrc2Dst;
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
	hlim::NodePort newOutput = handle.getOutput();
	{
		auto it = outputsTranslated.find(handle.getOutput());
		if (it != outputsTranslated.end())
			newOutput = it->second;
		else
			newOutput.node = mapSrc2Dst.find(handle.getOutput().node)->second;
	}

	// Force output's existence throughout optimization
	auto *pin = simCircuit.createNode<hlim::Node_Pin>(false, true, false);
	pin->recordStackTrace();
	pin->moveToGroup(simCircuit.getRootNodeGroup());
	pin->connect(newOutput);

	//visualize(simCircuit, "/tmp/circuit_04");

	// optimize
	simCircuit.postprocess(gtry::hlim::DefaultPostprocessing{});

	// Reestablish output from pin
	newOutput = pin->getDriver(0);

	//visualize(simCircuit, "/tmp/circuit_05");

	// Run simulation
	sim::SimulatorCallbacks ignoreCallbacks;
	sim::ReferenceSimulator simulator(false);
	simulator.compileProgram(simCircuit);
	simulator.powerOn();

	// Fetch result
	state = simulator.getValueOfOutput(newOutput);
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor)
{
	HCL_ASSERT_HINT(false, "Simulation coroutine attempted to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil)
{
	HCL_ASSERT_HINT(false, "Simulation coroutine attempted to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock)
{
	HCL_ASSERT_HINT(false, "Simulation coroutine attempted to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitChange &waitChange)
{
	HCL_ASSERT_HINT(false, "Simulation coroutine attempted to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitStable &waitStable)
{
	HCL_ASSERT_HINT(false, "Simulation coroutine attempted to run (and suspend) outside of simulation!");
}

void ConstructionTimeSimulationContext::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
}

void ConstructionTimeSimulationContext::onWarning(const hlim::BaseNode *src, std::string msg)
{
}

void ConstructionTimeSimulationContext::onAssert(const hlim::BaseNode *src, std::string msg)
{
}

bool ConstructionTimeSimulationContext::hasAuxData(std::string_view key) const
{
	HCL_ASSERT_HINT(false, "Query for aux data outside of simulation");
}

std::any& ConstructionTimeSimulationContext::registerAuxData(std::string_view key, std::any data)
{
	HCL_ASSERT_HINT(false, "Registration of aux data outside of simulation");
}

std::any& ConstructionTimeSimulationContext::getAuxData(std::string_view key)
{
	HCL_ASSERT_HINT(false, "Query for aux data outside of simulation");
}


}
