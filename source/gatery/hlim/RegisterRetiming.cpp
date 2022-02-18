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

#include "RegisterRetiming.h"

#include "Circuit.h"
#include "Subnet.h"
#include "SignalDelay.h"
#include "Node.h"
#include "NodePort.h"
#include "GraphTools.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_Compare.h"
#include "coreNodes/Node_Rewire.h"
#include "coreNodes/Node_Arithmetic.h"
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemPort.h"
#include "supportNodes/Node_RegSpawner.h"
#include "../utils/Enumerate.h"
#include "../utils/Zip.h"

#include "../simulation/ReferenceSimulator.h"

#include "postprocessing/MemoryDetector.h"


#include "../export/DotExport.h"

#include <sstream>
#include <iostream>

//#define DEBUG_OUTPUT

namespace gtry::hlim {

/**
 * @brief Determines the exact area to be forward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param output The output that shall receive a register.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedForward(Circuit &circuit, Subnet &area, NodePort output, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, 
								std::set<Node_RegSpawner*> &regSpawnersToSpawn, 
								std::set<NodePort> &regSpawnersToRegistersToBeRemoved,
								bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<NodePort> openList;
	openList.push_back(output);


#ifdef DEBUG_OUTPUT
    auto writeSubnet = [&]{
		Subnet subnet = areaToBeRetimed;
		subnet.dilate(true, true);

        DotExport exp("retiming_area.dot");
        exp(circuit, subnet.asConst());
        exp.runGraphViz("retiming_area.svg");
    };
#endif

	while (!openList.empty()) {
		auto nodePort = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(nodePort.node)) continue;
		if (registersToBeRemoved.contains((Node_Register*)nodePort.node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(nodePort.node)) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals leave the specified operation area through node " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ", id " << nodePort.node->getId()
				<< ") without passing a register that can be retimed forward. Note that registers with enable signals can't be retimed yet.\n"
				<< "First node outside the operation area from:\n" << nodePort.node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}

		auto *regSpawner = dynamic_cast<Node_RegSpawner*>(nodePort.node);

		// We may not want to retime nodes to which references are still being held
		// References to node spawners are ok (?)
		if (nodePort.node->hasRef() && !ignoreRefs && !regSpawner) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node to which references are still being held " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ", id " << nodePort.node->getId() << ").\n"
				<< "Node with references from:\n" << nodePort.node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}			

		// Check that everything is using the same clock.
		for (auto *c : nodePort.node->getClocks()) {
			if (c != nullptr) {
				if (clock == nullptr) {
					clock = c;
					clockGivingNode = nodePort.node;
				} else {
					if (clock != c) {
						if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

						std::stringstream error;

						error 
							<< "An error occurred attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
							<< "Node from:\n" << output.node->getStackTrace() << "\n";

						error 
							<< "The fanning-in signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ").\n"
							<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
							<< "Second node from:\n" << nodePort.node->getStackTrace() << "\n";

						HCL_ASSERT_HINT(false, error.str());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		if (nodePort.node->hasSideEffects()) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occurred attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node with side effects " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ", id " << nodePort.node->getId()
				<< ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << nodePort.node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		


		// Everything seems good with this node, so proceed
		if (regSpawner) {  // Register spawners spawn registers, so stop here

			regSpawnersToSpawn.insert(regSpawner);
			regSpawnersToRegistersToBeRemoved.insert(nodePort);
		} else
		if (auto *reg = dynamic_cast<Node_Register*>(nodePort.node)) {  // Registers need special handling
			if (registersToBeRemoved.contains(reg)) continue;

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (!reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_FORWARD) || reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr) {
				// Retime over this register. This means the enable port is part of the fan-in and we also need to search it for a register.
				areaToBeRetimed.add(nodePort.node);
				for (unsigned i : {Node_Register::DATA, Node_Register::ENABLE}) {
					auto driver = reg->getDriver(i);
					if (driver.node != nullptr)
						openList.push_back(driver);
				}
			} else {
				// Found a register to retime forward, stop here.
				registersToBeRemoved.insert(reg);

				// It is important to not add the register to the area to be retimed!
				// If the register is part of a loop that is part of the retiming area, 
				// the input to the register effectively leaves the retiming area, thus forcing the placement
				// of a new register with a new reset value. The old register is bypassed thus
				// replacing the old register with the new register.
				// In other words, for registers that are completely embeddded in the retiming area,
				// this mechanism implicitely advances the reset value by one iteration which is necessary 
				// because we can not retime a register out of the reset-pin-path.
			}
		} else {
			// Regular nodes just get added to the retiming area and their inputs are further explored
			areaToBeRetimed.add(nodePort.node);
			for (size_t i : utils::Range(nodePort.node->getNumInputPorts())) {
				auto driver = nodePort.node->getDriver(i);
				if (driver.node != nullptr)
					if (driver.node->getOutputConnectionType(driver.port).interpretation != ConnectionType::DEPENDENCY)
						openList.push_back(driver);
			}

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(nodePort.node)) { // If it is a memory port attempt to retime entire memory
				auto *memory = memPort->getMemory();
				areaToBeRetimed.add(memory);
			
				// add all memory ports to open list
				for (auto np : memory->getPorts()) 
					openList.push_back(np);
			 }
		}
	}

	return true;
}

bool retimeForwardToOutput(Circuit &circuit, Subnet &area, NodePort output, const RetimingSetting &settings)
{
	Subnet areaToBeRetimed;
	std::set<Node_Register*> registersToBeRemoved;
	std::set<Node_RegSpawner*> regSpawnersToSpawn;
	std::set<NodePort> regSpawnersToRegistersToBeRemoved;

	if (!determineAreaToBeRetimedForward(circuit, area, output, areaToBeRetimed, registersToBeRemoved, regSpawnersToSpawn, regSpawnersToRegistersToBeRemoved, settings.ignoreRefs, settings.failureIsError))
		return false;

	/*
	{
		std::array<const BaseNode*,1> arr{output.node};
		ConstSubnet csub = ConstSubnet::allNecessaryForNodes({}, arr);
		csub.add(output.node);

		DotExport exp("DriversOfOutput.dot");
		exp(circuit, csub);
		exp.runGraphViz("DriversOfOutput.svg");
	}
	{
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, areaToBeRetimed.asConst());
		exp.runGraphViz("areaToBeRetimed.svg");
	}
	*/

	std::set<hlim::NodePort> outputsLeavingRetimingArea;
	// Find every output leaving the area
	for (auto n : areaToBeRetimed)
		for (auto i : utils::Range(n->getNumOutputPorts()))
			for (auto np : n->getDirectlyDriven(i))
				if (!areaToBeRetimed.contains(np.node)) {
					outputsLeavingRetimingArea.insert({.node = n, .port = i});
					break;
				}

	if (regSpawnersToSpawn.size() > 1) {
		std::cout << "WARNING: Registers for retiming to a single location are sourced from " << regSpawnersToSpawn.size() << " different register spawners. This is usually a mistake." << std::endl;
		std::cout << "Register spawners: " << std::endl;
		for (auto spawner : regSpawnersToSpawn) {
			std::cout << "Node_RegSpawner id: " << spawner->getId() << " from:\n"
				<< spawner->getStackTrace() << std::endl;
		}
	}

	// Spawn register spawners
	for (auto *spawner : regSpawnersToSpawn) {
		auto regs = spawner->spawnForward();
		// Add new regs to area subnet to keep that up to date
		for (auto r : regs) area.add(r);
	}

	// Collect subset of spawned registers for removal
	for (auto spawnerOutput : regSpawnersToRegistersToBeRemoved) {
		const auto &driven = spawnerOutput.node->getDirectlyDriven(spawnerOutput.port);
		HCL_ASSERT(driven.size() == 1);
		HCL_ASSERT(driven.front().port == Node_Register::DATA);
		auto *reg = dynamic_cast<Node_Register*>(driven.front().node);
		HCL_ASSERT(reg != nullptr);

		registersToBeRemoved.insert(reg);
	}


	if (registersToBeRemoved.empty()) // no registers found to retime, probably everything is constant, so no clock available
		return false;

	HCL_ASSERT(!registersToBeRemoved.empty());
	auto *clock = (*registersToBeRemoved.begin())->getClocks()[0];

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileStaticEvaluation(circuit, {outputsLeavingRetimingArea});
    simulator.powerOn();

	auto arr = std::array{output};
	auto combinatoricallyDrivenArea = Subnet::allDrivenCombinatoricallyByOutputs(arr);

	// Insert registers
	for (auto np : outputsLeavingRetimingArea) {
		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clock);
		// Setup input data
		reg->connectInput(Node_Register::DATA, np);
		// add to the node group of its new driver
		reg->moveToGroup(np.node->getGroup());

		// allow further retiming by default
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

		if (settings.downstreamDisableForwardRT) {
			bool isDownstream = combinatoricallyDrivenArea.contains(np.node);

			if (!isDownstream)
				reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		} else {
			reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		}
		
		area.add(reg);
		if (settings.newNodes) settings.newNodes->add(reg);


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
			if (settings.newNodes) settings.newNodes->add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}

		// Find all signals leaving the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (!areaToBeRetimed.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}

	// Bypass input registers for the retimed nodes
	for (auto *reg : registersToBeRemoved) {
		const auto &allDriven = reg->getDirectlyDriven(0);
		for (int i = (int)allDriven.size()-1; i >= 0; i--) {
			if (areaToBeRetimed.contains(allDriven[i].node)) {
				allDriven[i].node->rewireInput(allDriven[i].port, reg->getDriver((unsigned)Node_Register::DATA));
			}	
		}
	}

	return true;
}



void retimeForward(Circuit &circuit, Subnet &subnet)
{
	bool done = false;
	while (!done) {

		// estimate signal delays
		hlim::SignalDelay delays;
		delays.compute(subnet);

		// Find critical output
		hlim::NodePort criticalOutput;
		size_t criticalBit = ~0u;
		float criticalTime = 0.0f;
		for (auto &n : subnet)
			for (auto i : utils::Range(n->getNumOutputPorts())) {
				hlim::NodePort np = {.node = n, .port = i};
				auto d = delays.getDelay(np);
				for (auto i : utils::Range(d.size())) {   
					if (d[i] > criticalTime) {
						criticalTime = d[i];
						criticalOutput = np;
						criticalBit = i;
					}
				}
			}
/*
		{
            DotExport exp("signalDelays.dot");
            exp(circuit, (hlim::ConstSubnet &)subnet, delays);
            exp.runGraphViz("signalDelays.svg");


			hlim::ConstSubnet criticalPathSubnet;
			{
				hlim::NodePort np = criticalOutput;
				unsigned bit = criticalBit;
				while (np.node != nullptr) {
					criticalPathSubnet.add(np.node);
					unsigned criticalInputPort, criticalInputBit;
					np.node->estimateSignalDelayCriticalInput(delays, np.port, bit, criticalInputPort, criticalInputBit);
					if (criticalInputPort == ~0u)
						np.node = nullptr;
					else {
						np = np.node->getDriver(criticalInputPort);
						bit = criticalInputBit;
					}
				}
			}

            DotExport exp2("criticalPath.dot");
            exp2(circuit, criticalPathSubnet, delays);
            exp2.runGraphViz("criticalPath.svg");
		}
*/
		// Split in half
		float splitTime = criticalTime * 0.5f;

		std::cout << "Critical path time: " << criticalTime << " Attempting to split at " << splitTime << std::endl;

		// Trace back critical path to find point where to retime register to
		hlim::NodePort retimingTarget;
		{
			hlim::NodePort np = criticalOutput;
			size_t bit = criticalBit;
			while (np.node != nullptr) {

				float thisTime = delays.getDelay(np)[bit];
				if (thisTime < splitTime) {
					retimingTarget = np;
					break;
				}

				size_t criticalInputPort, criticalInputBit;
				np.node->estimateSignalDelayCriticalInput(delays, np.port, bit, criticalInputPort, criticalInputBit);
				if (criticalInputPort == ~0u)
					np.node = nullptr;
				else {
					float nextTime = delays.getDelay(np.node->getDriver(criticalInputPort))[criticalInputBit];
					if ((thisTime + nextTime) * 0.5f < splitTime) {
					//if (nextTime < splitTime) {
						retimingTarget = np;
						break;
					}

					np = np.node->getDriver(criticalInputPort);
					bit = criticalInputBit;
				}
			}
		}

		if (retimingTarget.node != nullptr && dynamic_cast<Node_Register*>(retimingTarget.node) == nullptr) {
			done = !retimeForwardToOutput(circuit, subnet, retimingTarget, {.failureIsError=false});
		} else
			done = true;
	
// For debugging
//circuit.optimizeSubnet(subnet);
	}
}





/**
 * @brief Determines the exact area to be backward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param output The output that shall receive a register.
 * @param retimeableWritePorts List of write ports that may be retimed individually without retiming all other ports as well.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedBackward(Circuit &circuit, const Subnet &area, NodePort output, const std::set<Node_MemPort*> &retimeableWritePorts, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<NodePort> openList;
	for (auto np : output.node->getDirectlyDriven(output.port))
		openList.push_back(np);


#ifdef DEBUG_OUTPUT
    auto writeSubnet = [&]{
		{
        	DotExport exp("areaToBeRetimed.dot");
        	exp(circuit, areaToBeRetimed.asConst());
        	exp.runGraphViz("areaToBeRetimed.svg");	
		}

		Subnet subnet = areaToBeRetimed;
		subnet.dilate(false, true);
		subnet.dilate(false, true);
		subnet.dilate(false, true);
		subnet.dilate(true, true);

        DotExport exp("retiming_area.dot");
        exp(circuit, subnet.asConst());
        exp.runGraphViz("retiming_area.svg");
    };
#endif

	Node_Register *enableGivingRegister = nullptr;
	std::optional<NodePort> registerEnable;

	while (!openList.empty()) {
		auto nodePort = openList.back();
		auto *node = nodePort.node;
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals leave the specified operation area through node " << node->getName() << " (" << node->getTypeName() 
				<< ") without passing a register that can be retimed backward. Note that registers with enable signals can't be retimed yet.\n"
				<< "First node outside the operation area from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}

		// We may not want to retime nodes to which references are still being held
		if (node->hasRef() && !ignoreRefs) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals are driving a node to which references are still being held " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
				<< "Node with references from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}			

		// Check that everything is using the same clock.
		for (auto *c : node->getClocks()) {
			if (c != nullptr) {
				if (clock == nullptr) {
					clock = c;
					clockGivingNode = node;
				} else {
					if (clock != c) {
						if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

						std::stringstream error;

						error 
							<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
							<< "Node from:\n" << output.node->getStackTrace() << "\n";

						error 
							<< "The fanning-out signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << node->getName() << " (" << node->getTypeName() << ").\n"
							<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
							<< "Second node from:\n" << node->getStackTrace() << "\n";

						HCL_ASSERT_HINT(false, error.str());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		if (node->hasSideEffects()) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals are driving a node with side effects " << node->getName() << " (" << node->getTypeName() << ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		

		// Everything seems good with this node, so proceeed

		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling

			if (nodePort.port != (size_t)Node_Register::DATA) {
				if (!failureIsError) return false;
#ifdef DEBUG_OUTPUT
writeSubnet();
#endif
				std::stringstream error;

				error 
					<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
					<< "Node from:\n" << output.node->getStackTrace() << "\n";

				error 
					<< "The fanning-out signals are driving a non-data port of a register.\n    Register: " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
					<< "    From:\n" << node->getStackTrace() << "\n";

				HCL_ASSERT_HINT(false, error.str());
			}

			if (registersToBeRemoved.contains(reg)) continue;

			if (registerEnable) {
				if (reg->getNonSignalDriver(Node_Register::ENABLE) != *registerEnable) {
					if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif
					std::stringstream error;

					error 
						<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
						<< "Node from:\n" << output.node->getStackTrace() << "\n";

					error 
						<< "The fanning-out signals are driving register with different enables.\n    Register 1: " << node->getName() << " (" << node->getTypeName() << ", id " << enableGivingRegister->getId() << ").\n"
						<< "    From:\n" << node->getStackTrace() << "\n"
						<< "    Register 2: " << enableGivingRegister->getName() << " (" << enableGivingRegister->getTypeName() << ", id " << enableGivingRegister->getId() << ").\n"
						<< "    From:\n" << enableGivingRegister->getStackTrace() << "\n";

					HCL_ASSERT_HINT(false, error.str());
				}
			}

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (!reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_BACKWARD)) {
				// Retime over this register.
				areaToBeRetimed.add(node);
				for (size_t i : utils::Range(node->getNumOutputPorts()))
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np);
			} else {
				// Found a register to retime backward, stop here.
				registersToBeRemoved.insert(reg);

				registerEnable = reg->getNonSignalDriver(Node_Register::ENABLE);
				enableGivingRegister = reg;

				// It is important to not add the register to the area to be retimed!
				// If the register is part of a loop that is part of the retiming area, 
				// the output of the register effectively leaves the retiming area, thus forcing the placement
				// of a new register and a reset value check.
			}
		} else {
			// Regular nodes just get added to the retiming area and their outputs are further explored
			areaToBeRetimed.add(node);
			for (size_t i : utils::Range(node->getNumOutputPorts()))
				if (node->getOutputConnectionType(i).interpretation != ConnectionType::DEPENDENCY)
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np);

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port		 	
				auto *memory = memPort->getMemory();

				// Check if it is a write port that will be fixed later on
				if (retimeableWritePorts.contains(memPort)) {

					// It is a write port that may be retimed back wrt. to the read ports of the same memory because the change wrt. the read ports will be fixed with RMW logic later on.
					// However, the order wrt. other write ports must not change, so we need to retime all write ports of the same memory (and later on build RMW logic for all of them).

					// add all memory *write* ports to open list
					for (auto np : memory->getPorts()) {
						auto *otherMemPort = dynamic_cast<Node_MemPort*>(np.node);
						if (otherMemPort->isWritePort())
							openList.push_back(np);
					}

				} else {
					// attempt to retime entire memory
					areaToBeRetimed.add(memory);
				
					// add all memory ports to open list
					for (auto np : memory->getPorts()) 
						openList.push_back(np);
				}
			 }
		}
	}

	if (registerEnable) {
		if (registerEnable->node != nullptr) {
			if (areaToBeRetimed.contains(registerEnable->node)) {
				if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
writeSubnet();
#endif
				std::stringstream error;

				error 
					<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
					<< "Node from:\n" << output.node->getStackTrace() << "\n";

				error 
					<< "The fanning-out signals are driving register with enable signals that are driven from within the area that is to be retimed.\n"
					<< "    Register: " << enableGivingRegister->getName() << " (" << enableGivingRegister->getTypeName() << ", id " << enableGivingRegister->getId() << ").\n"
					<< "    From:\n" << enableGivingRegister->getStackTrace() << "\n";

				HCL_ASSERT_HINT(false, error.str());
			}
		}
	}

	return true;
}

bool retimeBackwardtoOutput(Circuit &circuit, Subnet &area, const std::set<Node_MemPort*> &retimeableWritePorts,
                        Subnet &retimedArea, NodePort output, bool ignoreRefs, bool failureIsError, Subnet *newNodes)
{

	// In case of multiple nodes being driven, pop in a single signal node so that this signal node can be part of the retiming area and do the broadcast
	if (output.node->getDirectlyDriven(output.port).size() > 1) {
		auto consumers = output.node->getDirectlyDriven(output.port);

		auto *sig = circuit.createNode<Node_Signal>();
		sig->recordStackTrace();
		sig->connectInput(output);
		sig->moveToGroup(output.node->getGroup());
		area.add(sig);

		for (auto c : consumers)
			c.node->rewireInput(c.port, {.node=sig, .port=0ull});
	}

	std::set<Node_Register*> registersToBeRemoved;
	if (!determineAreaToBeRetimedBackward(circuit, area, output, retimeableWritePorts, retimedArea, registersToBeRemoved, ignoreRefs, failureIsError))
		return false;
/*
	{
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, retimedArea.asConst());
		exp.runGraphViz("areaToBeRetimed.svg");
	}
*/
	if (retimedArea.empty()) return true; // immediately hit a register, so empty retiming area, nothing to do.

	std::set<hlim::NodePort> outputsEnteringRetimingArea;
	// Find every output entering the area
	for (auto n : retimedArea)
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node != nullptr && !outputIsDependency(driver) && !retimedArea.contains(driver.node))
				outputsEnteringRetimingArea.insert(driver);
		}

	std::set<hlim::NodePort> outputsLeavingRetimingArea;
	// Find every output leaving the area
	for (auto n : retimedArea)
		for (auto i : utils::Range(n->getNumOutputPorts()))
			for (auto np : n->getDirectlyDriven(i))
				if (!retimedArea.contains(np.node)) {
					outputsLeavingRetimingArea.insert({.node = n, .port = i});
					break;
				}

	// Find the clock domain. Try a register first.
	Clock *clock = nullptr;
	if (!registersToBeRemoved.empty()) {
		clock = (*registersToBeRemoved.begin())->getClocks()[0];
	} else {
		// No register, this must be a RMW loop, find a write port and grab clock from there.
		for (auto wp : retimeableWritePorts) 
			if (retimedArea.contains(wp)) {
				clock = wp->getClocks()[0];
				break;
			}
	}

	HCL_ASSERT(clock != nullptr);

	// Run a simulation to determine the reset values of the registers that will be removed to check the validity of removing them
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileProgram(circuit, {outputsLeavingRetimingArea}, true);
    simulator.powerOn();

	for (auto reg : registersToBeRemoved) {

		auto resetDriver = reg->getNonSignalDriver(Node_Register::RESET_VALUE);
		if (resetDriver.node != nullptr) {
			auto resetValue = evaluateStatically(circuit, resetDriver);
			auto inputValue = simulator.getValueOfOutput(reg->getDriver(0));

			HCL_ASSERT(resetValue.size() == inputValue.size());
			
			if (!sim::canBeReplacedWith(resetValue, inputValue)) {
				if (!failureIsError) return false;

				std::stringstream error;

				error 
					<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
					<< "Node from:\n" << output.node->getStackTrace() << "\n";

				error 
					<< "One of the registers that would have to be removed " << reg->getName() << " (" << reg->getTypeName() <<  ", id " << reg->getId() << ") can not be retimed because its reset value is not compatible with its input uppon reset.\n"
					<< "Register from:\n" << reg->getStackTrace() << "\n"
					<< "Reset value: " << resetValue << "\n"
					<< "Value uppon reset: " << inputValue << "\n";

				HCL_ASSERT_HINT(false, error.str());
			}
		}
	}


	NodePort enableSignal;
	if (!registersToBeRemoved.empty())
		enableSignal = (*registersToBeRemoved.begin())->getDriver(Node_Register::ENABLE);

	// Insert registers
	for (auto np : outputsEnteringRetimingArea) {

		// Don't insert registers on constants
		if (dynamic_cast<Node_Constant*>(np.node)) continue;
		// Don't insert registers on signals leading to constants
		if (auto *signal = dynamic_cast<Node_Signal*>(np.node))
			if (dynamic_cast<Node_Constant*>(signal->getNonSignalDriver(0).node)) continue;

		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clock);
		// Setup input data
		reg->connectInput(Node_Register::DATA, np);
		// Connect to same enable as all the removed registers
		reg->connectInput(Node_Register::ENABLE, enableSignal);
		// add to the node group of its new driver
		reg->moveToGroup(np.node->getGroup());
		// allow further retiming by default
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		
		area.add(reg);
		if (newNodes) newNodes->add(reg);

		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
			if (newNodes) newNodes->add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}


		// Find all signals entering the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (retimedArea.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}

	// Remove output registers that have now been retimed forward
	for (auto *reg : registersToBeRemoved)
		reg->bypassOutputToInput(0, (unsigned)Node_Register::DATA);

	return true;
}

void ReadModifyWriteHazardLogicBuilder::build(bool useMemory)
{
	size_t maxLatencyCompensation = 0;
	for (const auto &wp : m_writePorts)
		maxLatencyCompensation = std::max(maxLatencyCompensation, wp.latencyCompensation);

	std::vector<Node_Memory*> newMemoriesBuild;

	if (maxLatencyCompensation == 0) return;
	if (m_readPorts.empty()) return;
	if (m_writePorts.empty()) return;


	size_t totalDataWidth = getOutputWidth(m_readPorts.front().dataOutOutputDriver);
	for (auto &rdPort : m_readPorts)
		HCL_DESIGNCHECK_HINT(getOutputWidth(rdPort.dataOutOutputDriver) == totalDataWidth, "The RMW hazard logic builder requires all data busses of all read ports to be the same width.");

	for (auto &wrPort : m_writePorts)
		HCL_DESIGNCHECK_HINT(getOutputWidth(wrPort.dataInInputDriver) == totalDataWidth, "The RMW hazard logic builder requires the data busses of the write ports to be the same width as the read ports.");


	// First, determine reset values for all places where we may want to insert registers.
	std::map<NodePort, sim::DefaultBitVectorState> resetValues;
	for (auto &rdPort : m_readPorts) {
		resetValues.insert({rdPort.addrInputDriver, {}});
	}
	determineResetValues(resetValues);


	// Find a minimal partitioning of the data such that each byte-enable-able symbol of each write port spans a whole multiple of these words.
	std::vector<DataWord> dataWords = findDataPartitionining();

	NodePort ringBufferCounter;
	if (useMemory) {
		// In memory mode, we need one write pointer for all ring buffers
		ringBufferCounter = buildRingBufferCounter(maxLatencyCompensation);

		for (auto &dw : dataWords)
			dw.representationWidth = getOutputWidth(ringBufferCounter);
	} else {
		for (auto &dw : dataWords)
			dw.representationWidth = dw.width;
	}


	// Setup write port structures
	struct WritePortSignals {
		NodePort wpIdx;
		std::vector<NodePort> words;
		std::vector<Node_Memory*> ringbuffers;
	};
	std::vector<WritePortSignals> allWrPortSignals;
	allWrPortSignals.resize(m_writePorts.size());

	if (useMemory) {
		for (auto wrIdx : utils::Range(m_writePorts.size())) {
			// Split the input into individual words
			auto words = splitWords(m_writePorts[wrIdx].dataInInputDriver, dataWords);

			// Build one ringbuffer per write port and per word, since they must be read individually.
			allWrPortSignals[wrIdx].ringbuffers.resize(dataWords.size());
			allWrPortSignals[wrIdx].words.resize(dataWords.size());
			for (auto wordIdx : utils::Range(dataWords.size())) {
				allWrPortSignals[wrIdx].ringbuffers[wordIdx] = buildWritePortRingBuffer(words[wordIdx], ringBufferCounter);
				newMemoriesBuild.push_back(allWrPortSignals[wrIdx].ringbuffers[wordIdx]);

				// The data to pass through the registers is the write pointer where the data is stored...
				allWrPortSignals[wrIdx].words[wordIdx] = ringBufferCounter;
			}

			// ... as well as the index of the write port if there are multiple.
			if (m_writePorts.size() > 1) {
				auto idxWidth = utils::Log2C(m_writePorts.size());
				sim::DefaultBitVectorState state;
				state.resize(idxWidth);
				state.setRange(sim::DefaultConfig::DEFINED, 0, idxWidth);
				state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, idxWidth, wrIdx);

				auto *constNode = m_circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
				constNode->moveToGroup(m_newNodesNodeGroup);
				constNode->recordStackTrace();

				allWrPortSignals[wrIdx].wpIdx = {.node = constNode, .port = 0ull};
			}
		}
	} else {
		for (auto wrIdx : utils::Range(m_writePorts.size())) {
			// Just split data into words.
			allWrPortSignals[wrIdx].words = splitWords(m_writePorts[wrIdx].dataInInputDriver, dataWords);
			// .wpIdx not needed in non-memory mode
		}
	}

	// The rest has to be build for each read port individually
	for (auto rdPort : m_readPorts) {

		// Keep a list of per-word signals to update as we move through the stages
		struct PerWord {
			NodePort conflict;
			NodePort overrideData;
			NodePort overrideWpIdx;
		};
		std::vector<PerWord> wordSignals;
		wordSignals.resize(dataWords.size());



		// Build address shift register 
		std::vector<NodePort> rdPortAddrShiftReg;
		rdPortAddrShiftReg.resize(maxLatencyCompensation);
		rdPortAddrShiftReg[0] = rdPort.addrInputDriver;
		for (auto i : utils::Range<size_t>(1, maxLatencyCompensation))
			rdPortAddrShiftReg[i] = createRegister(rdPortAddrShiftReg[i-1], {});


		// Build each stage
		for (auto stageIdx : utils::Range(rdPortAddrShiftReg.size())) {
			auto &rdAddr = rdPortAddrShiftReg[stageIdx];

			// Build muxes in write port write order
			for (auto [wrPort, wrPortSignals] : utils::Zip(m_writePorts, allWrPortSignals)) {
				if (wrPort.latencyCompensation > stageIdx) {
					// Check whether a conflict can exists based on addr and enable
					NodePort conflict = buildConflictDetection(rdAddr, {}, wrPort.addrInputDriver, wrPort.enableInputDriver);

					for (auto wordIdx : utils::Range(dataWords.size())) {
						// Check whether a conflict exists for this word:
						NodePort wordConflict = andWithMaskBit(conflict, wrPort.enableMaskInputDriver, wordIdx);

						// Build multiplexers for each word to override data and idx if they exist.
						// buildConflictOr and buildConflictMux just wire through if one of the operands is a nullptr (as is the case for the first stage and for the idx in non-memory-mode).
						wordSignals[wordIdx].conflict = buildConflictOr(wordSignals[wordIdx].conflict, wordConflict);
						wordSignals[wordIdx].overrideData = buildConflictMux(wordSignals[wordIdx].overrideData, wrPortSignals.words[wordIdx], wordConflict);
						wordSignals[wordIdx].overrideWpIdx = buildConflictMux(wordSignals[wordIdx].overrideWpIdx, wrPortSignals.wpIdx, wordConflict);

						giveName(wordSignals[wordIdx].conflict, (boost::format("conflict_word_%d_stage_%d") % wordIdx % stageIdx).str());
						giveName(wordSignals[wordIdx].overrideData, (boost::format("bypass_data_word_%d_stage_%d") % wordIdx % stageIdx).str());
					}
				} else {
					// This write port needs less than the maximum latency compensation, usually because it already had a manually placed register in its path.
					// Skip the last multiplexers for this write port.
				}
			}

			// Add registers to each word
			// If using m_retimeToMux and useMemory, then we skip the last set of registers, put them after the read ports, and set the memory to write-first
			if ((stageIdx+1 < rdPortAddrShiftReg.size()) || !(m_retimeToMux && useMemory))
				for (auto wordIdx : utils::Range(dataWords.size())) {
					wordSignals[wordIdx].conflict = createRegister(wordSignals[wordIdx].conflict, {});
					wordSignals[wordIdx].overrideData = createRegister(wordSignals[wordIdx].overrideData, {});
					wordSignals[wordIdx].overrideWpIdx = createRegister(wordSignals[wordIdx].overrideWpIdx, {});
				}
		}

		// Finally, mux back to override what the read port appears to read

		// Fetch a list of all consumers (before we build consumers of our own) for later to rewire
		std::vector<NodePort> consumers = rdPort.dataOutOutputDriver.node->getDirectlyDriven(rdPort.dataOutOutputDriver.port);

		// Split What we read into words
		auto rpOutput = splitWords(rdPort.dataOutOutputDriver, dataWords);

		for (auto wordIdx : utils::Range(dataWords.size())) {
			NodePort overrideData;
			if (useMemory) {

				// Mux between write ports, if there are multiple
				if (m_writePorts.size() > 1) {
					auto *muxNode = m_circuit.createNode<Node_Multiplexer>(m_writePorts.size());
					muxNode->moveToGroup(m_newNodesNodeGroup);
					muxNode->recordStackTrace();
					muxNode->setComment("Mux between write port overrides from each write port.");
					muxNode->connectSelector(wordSignals[wordIdx].overrideWpIdx);

					for (auto wrIdx : utils::Range(m_writePorts.size())) {
						// For each word, read what each write port may have written there
						auto *readPort = m_circuit.createNode<hlim::Node_MemPort>(dataWords[wordIdx].width);
						readPort->moveToGroup(m_newNodesNodeGroup);
						readPort->recordStackTrace();
						readPort->connectMemory(allWrPortSignals[wrIdx].ringbuffers[wordIdx]);
						readPort->connectAddress(wordSignals[wordIdx].overrideData);

						// If using m_retimeToMux and useMemory, then we need to set the memory to write-first
						if (m_retimeToMux)
							readPort->orderAfter(allWrPortSignals[wrIdx].ringbuffers[wordIdx]->getLastPort());

						muxNode->connectInput(wrIdx, {.node = readPort, .port = (unsigned)hlim::Node_MemPort::Outputs::rdData});
					}

					overrideData = {.node = muxNode, .port=0ull};
				} else {
					auto *readPort = m_circuit.createNode<hlim::Node_MemPort>(dataWords[wordIdx].width);
					readPort->moveToGroup(m_newNodesNodeGroup);
					readPort->recordStackTrace();
					readPort->connectMemory(allWrPortSignals[0].ringbuffers[wordIdx]);
					readPort->connectAddress(wordSignals[wordIdx].overrideData);
					// If using m_retimeToMux and useMemory, then we need to set the memory to write-first
					if (m_retimeToMux)
						readPort->orderAfter(allWrPortSignals[0].ringbuffers[wordIdx]->getLastPort());

					overrideData = {.node = readPort, .port = (unsigned)hlim::Node_MemPort::Outputs::rdData};
				}

			} else 
				overrideData = wordSignals[wordIdx].overrideData;

			auto conflict = wordSignals[wordIdx].conflict;

			giveName(conflict, (boost::format("final_conflict_word_%d") % wordIdx).str());
			giveName(overrideData, (boost::format("final_override_data_word_%d") % wordIdx).str());


			// If using m_retimeToMux and useMemory, then we put the last set of registers here explicitely
			if (m_retimeToMux && useMemory) {
				conflict = createRegister(conflict, {});
				overrideData = createRegister(overrideData, {});
			}

			// Mux between actual read and forwarded written data
			auto *muxNode = m_circuit.createNode<Node_Multiplexer>(2);
			muxNode->moveToGroup(m_newNodesNodeGroup);
			muxNode->recordStackTrace();
			muxNode->setComment("If read and write addr match and read and write are enabled and write is not masked, forward write data to read output.");
			muxNode->connectSelector(conflict);
			muxNode->connectInput(0, rpOutput[wordIdx]);
			muxNode->connectInput(1, overrideData);
			rpOutput[wordIdx] = {.node = muxNode, .port=0ull};
		}
		
		NodePort data = joinWords(rpOutput);
		giveName(data, "hazard_corrected_data");
		
		// Rewire  all original consumers to use the new, potentially forwarded data
		for (auto np : consumers)
			np.node->rewireInput(np.port, data);		

		// If required, move one of the registers as close as possible to the mux to reduce critical path length
		if (m_retimeToMux && !useMemory) {
//visualize(m_circuit, "before_retiming_to_mux");
			for (auto wordIdx : utils::Range(dataWords.size())) {
				auto *muxNode = dynamic_cast<Node_Multiplexer*>(rpOutput[wordIdx].node);
				/// @todo: Add new nodes to subnet of new nodes
				Subnet area = Subnet::all(m_circuit);
				Subnet newNodes;
				for (auto i : {0, 2}) // don't retime memory read port register, only override data and conflict signal
					if (!dynamic_cast<Node_Register*>(muxNode->getNonSignalDriver(i).node))
						retimeForwardToOutput(m_circuit, area, muxNode->getDriver(i), {.ignoreRefs=true, .newNodes=&newNodes});

				for (auto n : newNodes)
					n->moveToGroup(m_newNodesNodeGroup);
			}
//visualize(m_circuit, "after_retiming_to_mux");
		}

	}


	for (auto m : newMemoriesBuild)
		formMemoryGroupIfNecessary(m_circuit, m);
//visualize(m_circuit, "after_rmw");
}



void ReadModifyWriteHazardLogicBuilder::determineResetValues(std::map<NodePort, sim::DefaultBitVectorState> &resetValues)
{
	std::set<NodePort> requiredNodePorts;

	for (auto &p : resetValues)
		if (p.first.node != nullptr)
			requiredNodePorts.insert(p.first);

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileStaticEvaluation(m_circuit, requiredNodePorts);
    simulator.powerOn();

	for (auto &p : resetValues)
		if (p.first.node != nullptr)
			p.second = simulator.getValueOfOutput(p.first);
}

NodePort ReadModifyWriteHazardLogicBuilder::createRegister(NodePort nodePort, const sim::DefaultBitVectorState &resetValue)
{
	if (nodePort.node == nullptr)
		return {};

	auto *reg = m_circuit.createNode<Node_Register>();
	reg->moveToGroup(m_newNodesNodeGroup);

	reg->recordStackTrace();
	reg->setClock(m_clockDomain);
	reg->connectInput(Node_Register::DATA, nodePort);
	reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);

	// If any input bit is defined uppon reset, add that as a reset value
	if (sim::anyDefined(resetValue, 0, resetValue.size())) {
		auto *resetConst = m_circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(nodePort).interpretation);
		resetConst->moveToGroup(m_newNodesNodeGroup);
		resetConst->recordStackTrace();
		resetConst->moveToGroup(reg->getGroup());
		reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
	}	
	return {.node = reg, .port = 0ull};
}

NodePort ReadModifyWriteHazardLogicBuilder::buildConflictDetection(NodePort rdAddr, NodePort rdEn, NodePort wrAddr, NodePort wrEn)
{
	auto *addrCompNode = m_circuit.createNode<Node_Compare>(Node_Compare::EQ);
	addrCompNode->moveToGroup(m_newNodesNodeGroup);
	addrCompNode->recordStackTrace();
	addrCompNode->setComment("Compare read and write addr for conflicts");
	addrCompNode->connectInput(0, rdAddr);
	addrCompNode->connectInput(1, wrAddr);

	NodePort conflict = {.node = addrCompNode, .port = 0ull};
//	m_circuit.appendSignal(conflict)->setName("conflict_same_addr");

	if (rdEn.node != nullptr) {
		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		logicAnd->moveToGroup(m_newNodesNodeGroup);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, conflict);
		logicAnd->connectInput(1, rdEn);
		conflict = {.node = logicAnd, .port = 0ull};
//		m_circuit.appendSignal(conflict)->setName("conflict_and_rdEn");
	}

	if (wrEn.node != nullptr) {
		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		logicAnd->moveToGroup(m_newNodesNodeGroup);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, conflict);
		logicAnd->connectInput(1, wrEn);
		conflict = {.node = logicAnd, .port = 0ull};
//		m_circuit.appendSignal(conflict)->setName("conflict_and_wrEn");
	}

	return conflict;
}

NodePort ReadModifyWriteHazardLogicBuilder::andWithMaskBit(NodePort input, NodePort mask, size_t maskBit)
{
	if (mask.node != nullptr) {
		auto *rewireNode = m_circuit.createNode<Node_Rewire>(1);
		rewireNode->moveToGroup(m_newNodesNodeGroup);
		rewireNode->recordStackTrace();
		rewireNode->connectInput(0, mask);
		rewireNode->changeOutputType({.interpretation = ConnectionType::BOOL, .width=1});
		rewireNode->setExtract(maskBit, 1);

		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		logicAnd->moveToGroup(m_newNodesNodeGroup);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, input);
		logicAnd->connectInput(1, {.node = rewireNode, .port = 0ull});

		input = {.node = logicAnd, .port = 0ull};
	}
	return input;
}

std::vector<NodePort> ReadModifyWriteHazardLogicBuilder::splitWords(NodePort data, NodePort mask)
{
	std::vector<NodePort> words;
	if (mask.node == nullptr) {
		words.resize(1);
		words[0] = data;
		return words;
	}

	size_t numWords = getOutputWidth(mask);
	HCL_ASSERT(getOutputWidth(data) % numWords == 0);

	size_t wordSize = getOutputWidth(data) / numWords;

	words.resize(numWords);
	for (auto i : utils::Range(numWords)) {
		auto *rewireNode = m_circuit.createNode<Node_Rewire>(1);
		rewireNode->moveToGroup(m_newNodesNodeGroup);
		rewireNode->recordStackTrace();
		rewireNode->setComment("Because of (byte) enable mask of write port, extract each (byte/)word and mux individually.");
		rewireNode->connectInput(0, data);
		rewireNode->changeOutputType(getOutputConnectionType(data));
		rewireNode->setExtract(i*wordSize, wordSize);
		NodePort individualWord = {.node = rewireNode, .port = 0ull};
//		m_circuit.appendSignal(individualWord)->setName(std::string("word_") + std::to_string(i));

		words[i] = individualWord;
	}
	return words;
}


std::vector<NodePort> ReadModifyWriteHazardLogicBuilder::splitWords(NodePort data, const std::vector<DataWord> &words)
{
	if (words.size() == 1)
		return {data};

	std::vector<NodePort> dataWords;
	dataWords.resize(words.size());
	for (auto i : utils::Range(dataWords.size())) {
		auto *rewireNode = m_circuit.createNode<Node_Rewire>(1);
		rewireNode->moveToGroup(m_newNodesNodeGroup);
		rewireNode->recordStackTrace();
		rewireNode->setComment("Because of (byte) enable mask of write port, extract each (byte/)word");
		rewireNode->connectInput(0, data);
		rewireNode->changeOutputType(getOutputConnectionType(data));
		rewireNode->setExtract(words[i].offset, words[i].width);
		NodePort individualWord = {.node = rewireNode, .port = 0ull};

		dataWords[i] = individualWord;
	}
	return dataWords;
}



NodePort ReadModifyWriteHazardLogicBuilder::joinWords(const std::vector<NodePort> &words)
{
	HCL_ASSERT(!words.empty());

	if (words.size() == 1)
		return words.front();

	auto *rewireNode = m_circuit.createNode<Node_Rewire>(words.size());
	rewireNode->moveToGroup(m_newNodesNodeGroup);
	rewireNode->recordStackTrace();
	rewireNode->setComment("Join individual words back together");
	for (auto i : utils::Range(words.size()))
		rewireNode->connectInput(i, words[i]);

	rewireNode->changeOutputType(getOutputConnectionType(words.front()));
	rewireNode->setConcat();
	NodePort joined = {.node = rewireNode, .port = 0ull};
	return joined;
}

std::vector<ReadModifyWriteHazardLogicBuilder::DataWord> ReadModifyWriteHazardLogicBuilder::findDataPartitionining()
{
	size_t totalDataWidth = getOutputWidth(m_readPorts.front().dataOutOutputDriver);

	std::vector<size_t> wordSizes;
	wordSizes.resize(m_writePorts.size());
	for (auto [wrPort, wordSize] : utils::Zip(m_writePorts, wordSizes)) {
		auto mask = wrPort.enableMaskInputDriver;
		if (mask.node == nullptr)
			wordSize = totalDataWidth;
		else {
			size_t numWords = getOutputWidth(mask);
			HCL_ASSERT(totalDataWidth % numWords == 0);
			wordSize = totalDataWidth / numWords;
		}
	}



	std::vector<DataWord> words;
	size_t lastSplit = 0;

	for (auto bitIdx : utils::Range(totalDataWidth)) {
		bool needsSplit = false;
		for (auto portSize : wordSizes)
			if ((bitIdx+1) % portSize == 0) {
				needsSplit = true;
				break;
			}

		if (needsSplit) {
			DataWord newWord;
			newWord.offset = lastSplit;
			newWord.width = bitIdx+1 - lastSplit;
			newWord.writePortEnableBit.resize(m_writePorts.size());
			for (auto portIdx : utils::Range(m_writePorts.size()))
				if (wordSizes[portIdx] == totalDataWidth)
					newWord.writePortEnableBit[portIdx] = ~0u;
				else
					newWord.writePortEnableBit[portIdx] = bitIdx / wordSizes[portIdx];

			words.push_back(std::move(newWord));
			lastSplit = bitIdx;
		}
	}
	
	return words;
}

NodePort ReadModifyWriteHazardLogicBuilder::buildConflictOr(NodePort a, NodePort b)
{
	if (a.node == nullptr) return b;
	if (b.node == nullptr) return a;

	auto *logicOr = m_circuit.createNode<Node_Logic>(Node_Logic::OR);
	logicOr->moveToGroup(m_newNodesNodeGroup);
	logicOr->recordStackTrace();
	logicOr->connectInput(0, a);
	logicOr->connectInput(1, b);
	return {.node = logicOr, .port = 0ull};
}

NodePort ReadModifyWriteHazardLogicBuilder::buildConflictMux(NodePort oldData, NodePort newData, NodePort conflict)
{
	if (oldData.node == nullptr) return newData;
	if (newData.node == nullptr) return oldData;

	auto *muxNode = m_circuit.createNode<Node_Multiplexer>(2);
	muxNode->moveToGroup(m_newNodesNodeGroup);
	muxNode->recordStackTrace();
	muxNode->connectSelector(conflict);
	muxNode->connectInput(0, oldData);
	muxNode->connectInput(1, newData);

	return {.node = muxNode, .port=0ull};
}

NodePort ReadModifyWriteHazardLogicBuilder::buildRingBufferCounter(size_t maxLatencyCompensation)
{
	auto counterWidth = utils::Log2C(maxLatencyCompensation+1);

	auto *reg = m_circuit.createNode<Node_Register>();
	reg->moveToGroup(m_newNodesNodeGroup);

	reg->recordStackTrace();
	reg->setClock(m_clockDomain);
	reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);


	sim::DefaultBitVectorState state;
	state.resize(counterWidth);
	state.setRange(sim::DefaultConfig::DEFINED, 0, counterWidth);
	state.clearRange(sim::DefaultConfig::VALUE, 0, counterWidth);

	auto *resetConst = m_circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
	resetConst->moveToGroup(m_newNodesNodeGroup);
	resetConst->recordStackTrace();
	reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});


	// build a one
	state.setRange(sim::DefaultConfig::VALUE, 0, 1);
	auto *constOne = m_circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
	constOne->moveToGroup(m_newNodesNodeGroup);
	constOne->recordStackTrace();


	auto *addNode = m_circuit.createNode<Node_Arithmetic>(Node_Arithmetic::ADD);
	addNode->moveToGroup(m_newNodesNodeGroup);
	addNode->recordStackTrace();
	addNode->connectInput(1, {.node = constOne, .port = 0ull});

	reg->connectInput(Node_Register::DATA, {.node = addNode, .port = 0ull});

	NodePort counter = {.node = reg, .port = 0ull};
	giveName(counter, "ringbuffer_write_pointer");

	addNode->connectInput(0, counter);

	return counter;
}

Node_Memory *ReadModifyWriteHazardLogicBuilder::buildWritePortRingBuffer(NodePort wordData, NodePort ringBufferCounter)
{
	auto wordWidth = getOutputWidth(wordData);
	auto counterWidth = getOutputWidth(ringBufferCounter);

	NodeGroup *memGroup;
	if (m_newNodesNodeGroup)
		memGroup = m_newNodesNodeGroup;
	else
		memGroup = m_circuit.getRootNodeGroup();
	
	auto *memory = m_circuit.createNode<Node_Memory>();
	memory->moveToGroup(memGroup);
	memory->recordStackTrace();
	memory->setNoConflicts();
	memory->setType(Node_Memory::MemType::SMALL, 1);
	memory->setName("read_write_hazard_bypass_ringbuffer");
	{
		sim::DefaultBitVectorState state;
		state.resize((1ull << counterWidth) * wordWidth);
		memory->setPowerOnState(std::move(state));
	}

	auto *writePort = m_circuit.createNode<hlim::Node_MemPort>(wordWidth);
	writePort->moveToGroup(m_newNodesNodeGroup);
	writePort->recordStackTrace();
	writePort->connectMemory(memory);
	writePort->connectAddress(ringBufferCounter);
	writePort->connectWrData(wordData);
	writePort->setClock(m_clockDomain);

	return memory;
}

void ReadModifyWriteHazardLogicBuilder::giveName(NodePort &nodePort, std::string name)
{
	auto *sig = m_circuit.appendSignal(nodePort);
	sig->setName(std::move(name));
	sig->moveToGroup(m_newNodesNodeGroup);
}


}

