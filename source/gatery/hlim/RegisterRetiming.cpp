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
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemPort.h"
#include "../utils/Enumerate.h"
#include "../utils/Zip.h"

#include "../simulation/ReferenceSimulator.h"


#include "../export/DotExport.h"

#include <sstream>
#include <iostream>

#define DEBUG_OUTPUT

namespace gtry::hlim {

/**
 * @brief Determines the exact area to be forward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedForward(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<BaseNode*> openList;
	openList.push_back(output.node);


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
		auto *node = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(node)) continue;
		if (registersToBeRemoved.contains((Node_Register*)node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals leave the specified operation area through node " << node->getName() << " (" << node->getTypeName() 
				<< ") without passing a register that can be retimed forward. Note that registers with enable signals can't be retimed yet.\n"
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
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node to which references are still being held " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
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
							<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
							<< "Node from:\n" << output.node->getStackTrace() << "\n";

						error 
							<< "The fanning-in signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << node->getName() << " (" << node->getTypeName() << ").\n"
							<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
							<< "Second node from:\n" << node->getStackTrace() << "\n";

						HCL_ASSERT_HINT(false, error.str());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		// Memory ports are handled separately below
		if (node->hasSideEffects() && dynamic_cast<Node_MemPort*>(node) == nullptr) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node with side effects " << node->getName() << " (" << node->getTypeName() << ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		

		// Everything seems good with this node, so proceeed

		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling
			if (registersToBeRemoved.contains(reg)) continue;

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (!reg->getFlags().containsAnyOf(Node_Register::ALLOW_RETIMING_FORWARD) || anchoredRegisters.contains(reg) || reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr) {
				// Retime over this register. This means the enable port is part of the fan-in and we also need to search it for a register.
				areaToBeRetimed.add(node);
				for (unsigned i : {Node_Register::DATA, Node_Register::ENABLE}) {
					auto driver = reg->getDriver(i);
					if (driver.node != nullptr)
						openList.push_back(driver.node);
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
			areaToBeRetimed.add(node);
			for (unsigned i : utils::Range(node->getNumInputPorts())) {
				auto driver = node->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver.node);
			}

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port (can only be a read port, attempt to retime entire memory)
				auto *memory = memPort->getMemory();
				areaToBeRetimed.add(memory);
			
				// add all memory ports to open list
				for (auto np : memory->getDirectlyDriven(0)) 
					openList.push_back(np.node);
			 }
		}
	}

	return true;
}

bool retimeForwardToOutput(Circuit &circuit, Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, bool ignoreRefs, bool failureIsError)
{
	Subnet areaToBeRetimed;
	std::set<Node_Register*> registersToBeRemoved;
	if (!determineAreaToBeRetimedForward(circuit, area, anchoredRegisters, output, areaToBeRetimed, registersToBeRemoved, ignoreRefs, failureIsError))
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
		reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);
		
		area.add(reg);


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
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
	std::set<Node_Register*> anchoredRegisters;

	// Anchor all registers driven by memory
	for (auto &n : subnet)
		if (auto *reg = dynamic_cast<Node_Register*>(n)) {
			bool drivenByMemory = false;
			for (auto nh : reg->exploreInput((unsigned)Node_Register::Input::DATA)) {
				if (nh.isNodeType<Node_MemPort>()) {
					drivenByMemory = true;
					break;
				}
				if (!nh.node()->isCombinatorial()) {
					nh.backtrack();
					continue;
				}
			}
			if (drivenByMemory)
				anchoredRegisters.insert(reg);
		}

	bool done = false;
	while (!done) {

		// estimate signal delays
		hlim::SignalDelay delays;
		delays.compute(subnet);

		// Find critical output
		hlim::NodePort criticalOutput;
		unsigned criticalBit = ~0u;
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
			unsigned bit = criticalBit;
			while (np.node != nullptr) {

				float thisTime = delays.getDelay(np)[bit];
				if (thisTime < splitTime) {
					retimingTarget = np;
					break;
				}

				unsigned criticalInputPort, criticalInputBit;
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
			done = !retimeForwardToOutput(circuit, subnet, anchoredRegisters, retimingTarget, false, false);
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
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param retimeableWritePorts List of write ports that may be retimed individually without retiming all other ports as well.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedBackward(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, const std::set<Node_MemPort*> &retimeableWritePorts, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<NodePort> openList;
	for (auto np : output.node->getDirectlyDriven(output.port))
		openList.push_back(np);


#ifdef DEBUG_OUTPUT
    auto writeSubnet = [&]{
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
		// Memory ports are handled separately below
		if (node->hasSideEffects() && dynamic_cast<Node_MemPort*>(node) == nullptr) {
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
					<< "The fanning-out signals are driving a non-data port of a register.\n    Register: " << node->getName() << " (" << node->getTypeName() << ", id " << enableGivingRegister->getId() << ").\n"
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
			if (!reg->getFlags().containsAnyOf(Node_Register::ALLOW_RETIMING_BACKWARD) || anchoredRegisters.contains(reg)) {
				// Retime over this register.
				areaToBeRetimed.add(node);
				for (unsigned i : utils::Range(node->getNumOutputPorts()))
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
			for (unsigned i : utils::Range(node->getNumOutputPorts()))
				for (auto np : node->getDirectlyDriven(i))
					openList.push_back(np);

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port		 	
				// Check if it is a write port that will be fixed later on
				if (retimeableWritePorts.contains(memPort)) {
					areaToBeRetimed.add(memPort);
				} else {
					// attempt to retime entire memory
					auto *memory = memPort->getMemory();
					areaToBeRetimed.add(memory);
				
					// add all memory ports to open list
					for (auto np : memory->getDirectlyDriven(0)) 
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

bool retimeBackwardtoOutput(Circuit &circuit, Subnet &area, const std::set<Node_Register*> &anchoredRegisters, const std::set<Node_MemPort*> &retimeableWritePorts,
                        Subnet &retimedArea, NodePort output, bool ignoreRefs, bool failureIsError)
{
	std::set<Node_Register*> registersToBeRemoved;
	if (!determineAreaToBeRetimedBackward(circuit, area, anchoredRegisters, output, retimeableWritePorts, retimedArea, registersToBeRemoved, ignoreRefs, failureIsError))
		return false;

	{
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, retimedArea.asConst());
		exp.runGraphViz("areaToBeRetimed.svg");
	}

	if (retimedArea.empty()) return true; // immediately hit a register, so empty retiming area, nothing to do.

	std::set<hlim::NodePort> outputsEnteringRetimingArea;
	// Find every output entering the area
	for (auto n : retimedArea)
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (!outputIsDependency(driver) && !retimedArea.contains(driver.node))
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
    simulator.compileProgram(circuit, {outputsLeavingRetimingArea});
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
		reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);
		
		area.add(reg);


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
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

	// Remove input registers that have now been retimed forward
	for (auto *reg : registersToBeRemoved)
		reg->bypassOutputToInput(0, (unsigned)Node_Register::DATA);

	return true;
}

/*
namespace rmw {

	struct WritePortInputRegisters {
		Node_Register *enableReg;
		Node_Register *addrReg;
	};

bool buildReadModifyWriteHazardLogic(Circuit &circuit, std::span<ReadPort> readPorts, std::span<WritePort> sortedWritePorts, NodeGroup *targetNodeGroup, Subnet &subnet, bool failureIsError)
{

	std::vector<std::vector<WritePortInputRegisters>> wrPortInputRegisters;
	wrPortInputRegisters.resize(sortedWritePorts.size());

	// Find the list of input registers for all write ports
	for (auto [wrPort, wrPortRegs] : utils::Zip(sortedWritePorts, wrPortInputRegisters)) {
		wrPortRegs.resize(wrPort.actBackwardsAmount);

		NodePort enableInput = wrPort.enableInput;
		NodePort addrInput = wrPort.addrInput;

		for (auto i : utils::Range(wrPort.actBackwardsAmount)) {

			auto driverEn = enableInput.node->getNonSignalDriver(enableInput.port);
			auto driverAddr = addrInput.node->getNonSignalDriver(addrInput.port);
			wrPortRegs[i].enableReg = dynamic_cast<Node_Register*>(driverEn.node);
			wrPortRegs[i].addrReg = dynamic_cast<Node_Register*>(driverAddr.node);

			if (wrPortRegs[i].enableReg == nullptr) {
				if (failureIsError) {
					std::stringstream error;
					error 
						<< "An error occured attempting to build rmw bypass logic. For the enable input to node " << wrPort.enableInput.node->getName() << " (" << wrPort.enableInput.node->getTypeName() << ", id " << wrPort.enableInput.node->getId() << ")"
						<< " only " << i-1 << " registers were found, but " << wrPort.actBackwardsAmount << " are required for the write port to appear to act this far back in time.\n"
						<< "Node from:\n" << wrPort.enableInput.node->getStackTrace() << "\n";
					HCL_DESIGNCHECK_HINT(false, error.str());
				} else return false;
			}
			if (wrPortRegs[i].addrReg == nullptr) {
				if (failureIsError) {
					std::stringstream error;
					error 
						<< "An error occured attempting to build rmw bypass logic. For the address input to node " << wrPort.addrInput.node->getName() << " (" << wrPort.addrInput.node->getTypeName() << ", id " << wrPort.addrInput.node->getId() << ")"
						<< " only " << i-1 << " registers were found, but " << wrPort.actBackwardsAmount << " are required for the write port to appear to act this far back in time.\n"
						<< "Node from:\n" << wrPort.addrInput.node->getStackTrace() << "\n";
					HCL_DESIGNCHECK_HINT(false, error.str());
				} else return false;
			}

			enableInput = {.node = wrPortRegs[i].enableReg, .port = Node_Register::DATA};
			addrInput = {.node = wrPortRegs[i].addrReg, .port = Node_Register::DATA};
		}
	}

	

}

}
*/


bool ReadModifyWriteHazardLogicBuilder::build(bool failureIsError)
{
	HCL_ASSERT(m_readLatency != ~0u);

	if (m_readLatency == 0) return true;


	// First, determine reset values for all places where we may want to insert registers.
	std::map<NodePort, sim::DefaultBitVectorState> resetValues;
	for (auto &wrPort : m_writePorts) {
		resetValues.insert({wrPort.addrInputDriver, {}});
		resetValues.insert({wrPort.enableInputDriver, {}});
		resetValues.insert({wrPort.enableMaskInputDriver, {}});
		resetValues.insert({wrPort.dataInInputDriver, {}});
	}

	for (auto &rdPort : m_readPorts) {
		resetValues.insert({rdPort.addrInputDriver, {}});
		resetValues.insert({rdPort.enableInputDriver, {}});
	}
	determineResetValues(resetValues);



	// Second, build shift registers for all write ports so they can be reused for each read port
	struct WritePortDelayedSignals {
		NodePort delayedAddrDriver;
		NodePort delayedEnableDriver;
		NodePort delayedMaskDriver;
		NodePort delayedWrDataDriver;
		std::vector<NodePort> delayedWrDataWords;
	};	

	std::vector<std::vector<WritePortDelayedSignals>> wrPortDelayedSignals;
	wrPortDelayedSignals.resize(m_writePorts.size());

	for (auto [wrPort, wrDelayed] : utils::Zip(m_writePorts, wrPortDelayedSignals)) {
		wrDelayed.resize(m_readLatency);

		WritePortDelayedSignals last;
		last.delayedAddrDriver = wrPort.addrInputDriver;
		last.delayedEnableDriver = wrPort.enableInputDriver;
		last.delayedMaskDriver = wrPort.enableMaskInputDriver;
		last.delayedWrDataDriver = wrPort.dataInInputDriver;

		for (auto &s : wrDelayed) {
			s.delayedAddrDriver = createRegister(last.delayedAddrDriver, resetValues[wrPort.addrInputDriver]);
			s.delayedEnableDriver = createRegister(last.delayedEnableDriver, resetValues[wrPort.enableInputDriver]);
			s.delayedMaskDriver = createRegister(last.delayedMaskDriver, resetValues[wrPort.enableMaskInputDriver]);
			s.delayedWrDataDriver = createRegister(last.delayedWrDataDriver, resetValues[wrPort.dataInInputDriver]);
			s.delayedWrDataWords = splitWords(s.delayedWrDataDriver, s.delayedMaskDriver);

			last = s;
		}
	}


	// Now loop over all read ports, build the shift register to delay addr and enable inputs, and build the actual conflict resolution
	for (auto &rdPort : m_readPorts) {

		NodePort addr = rdPort.addrInputDriver;
		NodePort enable = rdPort.enableInputDriver;

		// Build shift register of inputs
		for ([[maybe_unused]] auto i : utils::Range(m_readLatency)) {
			addr = createRegister(addr, resetValues[rdPort.addrInputDriver]);
			enable = createRegister(enable, resetValues[rdPort.enableInputDriver]);
		}

		// Fetch a list of all consumers (before we build consumers of our own) for later to rewire
		std::vector<NodePort> consumers = rdPort.dataOutOutputDriver.node->getDirectlyDriven(rdPort.dataOutOutputDriver.port);

		// Build sequence of multiplexers (should usually be short since number of write ports should be low)
		NodePort data = rdPort.dataOutOutputDriver;
		for (auto [wrPort, wrDelayed] : utils::Zip(m_writePorts, wrPortDelayedSignals)) {

			// Split by enable mask chunks
			std::vector<NodePort> dataWords = splitWords(data, wrPort.enableMaskInputDriver);

			// Walk from oldest to newest write and potentially override data.
			for (size_t i = wrDelayed.size()-1; i < wrDelayed.size(); i--) {

				for (auto wordIdx : utils::Range(dataWords.size())) {
					// Check whether a conflict exists for this word:
					NodePort conflict = buildConflictDetection(addr, enable, wrDelayed[i].delayedAddrDriver, wrDelayed[i].delayedEnableDriver, wrDelayed[i].delayedMaskDriver, wordIdx);

					// Mux between actual read and forwarded written data
					auto *muxNode = m_circuit.createNode<Node_Multiplexer>(2);
					m_newNodes.add(muxNode);
					muxNode->recordStackTrace();
					muxNode->setComment("If read and write addr match and read and write are enabled and write is not masked, forward write data to read output.");
					muxNode->connectSelector(conflict);
					muxNode->connectInput(0, dataWords[wordIdx]);
					muxNode->connectInput(1, wrDelayed[i].delayedWrDataWords[wordIdx]);

					NodePort muxOut = {.node = muxNode, .port=0ull};

					dataWords[wordIdx] = muxOut;

					// If required, move one of the registers as close as possible to the mux to reduce critical path length
					if (m_retimeToMux) {
						/// @todo: Add new nodes to subnet of new nodes
						Subnet area = Subnet::all(m_circuit);
						retimeForwardToOutput(m_circuit, area, {}, conflict, false, true);
					}

				}
			}

			data = joinWords(dataWords);
		}

		// Rewire  all original consumers to use the new, potentially forwarded data
		for (auto np : consumers)
			np.node->rewireInput(np.port, data);		
	}

	return true;
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
	m_newNodes.add(reg);

	reg->recordStackTrace();
	reg->setClock(m_clockDomain);
	reg->connectInput(Node_Register::DATA, nodePort);
	reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);

	// If any input bit is defined uppon reset, add that as a reset value
	if (sim::anyDefined(resetValue, 0, resetValue.size())) {
		auto *resetConst = m_circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(nodePort).interpretation);
		m_newNodes.add(resetConst);
		resetConst->recordStackTrace();
		resetConst->moveToGroup(reg->getGroup());
		reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
	}	
	return {.node = reg, .port = 0ull};
}

NodePort ReadModifyWriteHazardLogicBuilder::buildConflictDetection(NodePort rdAddr, NodePort rdEn, NodePort wrAddr, NodePort wrEn, NodePort mask, size_t maskBit)
{
	auto *addrCompNode = m_circuit.createNode<Node_Compare>(Node_Compare::EQ);
	m_newNodes.add(addrCompNode);
	addrCompNode->recordStackTrace();
	addrCompNode->setComment("Compare read and write addr for conflicts");
	addrCompNode->connectInput(0, rdAddr);
	addrCompNode->connectInput(1, wrAddr);

	NodePort conflict = {.node = addrCompNode, .port = 0ull};
//	m_circuit.appendSignal(conflict)->setName("conflict_same_addr");

	if (rdEn.node != nullptr) {
		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		m_newNodes.add(logicAnd);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, conflict);
		logicAnd->connectInput(1, rdEn);
		conflict = {.node = logicAnd, .port = 0ull};
//		m_circuit.appendSignal(conflict)->setName("conflict_and_rdEn");
	}

	if (wrEn.node != nullptr) {
		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		m_newNodes.add(logicAnd);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, conflict);
		logicAnd->connectInput(1, wrEn);
		conflict = {.node = logicAnd, .port = 0ull};
//		m_circuit.appendSignal(conflict)->setName("conflict_and_wrEn");
	}

	if (mask.node != nullptr) {
		auto *rewireNode = m_circuit.createNode<Node_Rewire>(1);
		m_newNodes.add(rewireNode);
		rewireNode->recordStackTrace();
		rewireNode->connectInput(0, mask);
		rewireNode->changeOutputType({.interpretation = ConnectionType::BOOL, .width=1});
		rewireNode->setExtract(maskBit, 1);

		auto *logicAnd = m_circuit.createNode<Node_Logic>(Node_Logic::AND);
		m_newNodes.add(logicAnd);
		logicAnd->recordStackTrace();
		logicAnd->connectInput(0, conflict);
		logicAnd->connectInput(1, {.node = rewireNode, .port = 0ull});
		conflict = {.node = logicAnd, .port = 0ull};
//		m_circuit.appendSignal(conflict)->setName("conflict_and_notMasked");
	}

	return conflict;
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
		m_newNodes.add(rewireNode);
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

NodePort ReadModifyWriteHazardLogicBuilder::joinWords(const std::vector<NodePort> &words)
{
	HCL_ASSERT(!words.empty());

	if (words.size() == 1)
		return words.front();

	auto *rewireNode = m_circuit.createNode<Node_Rewire>(words.size());
	m_newNodes.add(rewireNode);
	rewireNode->recordStackTrace();
	rewireNode->setComment("Join individual words back together");
	for (auto i : utils::Range(words.size()))
		rewireNode->connectInput(i, words[i]);

	rewireNode->changeOutputType(getOutputConnectionType(words.front()));
	rewireNode->setConcat();
	NodePort joined = {.node = rewireNode, .port = 0ull};
	return joined;
}

}

