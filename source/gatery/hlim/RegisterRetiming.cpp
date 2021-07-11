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
#include "Node.h"
#include "NodePort.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemPort.h"

#include "../simulation/ReferenceSimulator.h"


#include "../export/DotExport.h"

#include <sstream>

namespace gtry::hlim {

/*
void retimeRegisterForward(Subnet &area, Node_Register *reg, NodePort output)
{
	Subnet areaToBeRetimed;

	std::set<NodePort> retimedAreaOutputs;

	for (auto nh : reg->exploreOutput(0)) {
		auto driver = nh.node()->getDriver(nh.port);
		if (!area.contains(nh.node()) {
			retimedAreaOutputs.insert(driver);
			nh.backtrack();
			continue;
		} 

		if (driver == output) {
			nh.backtrack();
			continue;
		}

		if (nh.node->hasSideEffects()) {
			nh.backtrack();
			continue;
		}
		if (areaToBeRetimed.contains(nh.node())) continue;
		areaToBeRetimed.insert(nh.node());
	}
}
*/

/**
 * @brief Determines the exact area to be retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRetimed Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 */
void determineAreaToBeRetimed(const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRetimed, bool ignoreRefs = false)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<BaseNode*> openList;
	openList.push_back(output.node);

	while (!openList.empty()) {
		auto *node = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(node)) continue;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals leave the specified operation area through node " << node->getName() << " (" << node->getTypeName() 
				<< ") without passing a register that can be retimed forward. Note that registers with enable signals can't be retimed yet.\n"
				<< "First node outside the operation area from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}

		// We may not want to retime nodes to which references are still being held
		if (node->hasRef() && !ignoreRefs) {
			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node to which references are still being held " << node->getName() << " (" << node->getTypeName() << ").\n"
				<< "Node with references from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}			

		// Check that everything is using the same clock.
		for (auto *c : node->getClocks()) {
			if (clock == nullptr) {
				clock = c;
				clockGivingNode = node;
			} else {
				if (clock != c) {
					std::stringstream error;

					error 
						<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << "):\n"
						<< "Node from:\n" << output.node->getStackTrace() << "\n";

					error 
						<< "The fanning-in signals are driven by different clocks. Clocks differe between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << node->getName() << " (" << node->getTypeName() << ").\n"
						<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
						<< "Second node from:\n" << node->getStackTrace() << "\n";

					HCL_ASSERT_HINT(false, error.str());
				}
			}
		}

		// We can not retime nodes with a side effect
		if (node->hasSideEffects()) {
			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node with side effects " << node->getName() << " (" << node->getTypeName() << ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		

		// Everything seems good with this node, so proceeed

		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling
			if (registersToBeRetimed.contains(reg)) continue;

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (anchoredRegisters.contains(reg) || reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr) {
				// Retime over this register. This means the enable port is part of the fan-in and we also need to search it for a register.
				areaToBeRetimed.add(node);
				for (unsigned i : {Node_Register::DATA, Node_Register::ENABLE}) {
					auto driver = reg->getDriver(i);
					if (driver.node != nullptr)
						openList.push_back(driver.node);
				}
			} else {
				// Found a register to retime forward, stop here.
				registersToBeRetimed.insert(reg);
			}
		} else {
			// Regular nodes just get added to the retiming area and their inputs are further explored
			areaToBeRetimed.add(node);
			for (unsigned i : utils::Range(node->getNumInputPorts())) {
				auto driver = node->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver.node);
			}
		}
	}
}

void retimeForwardToOutput(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, bool ignoreRefs)
{
	Subnet areaToBeRetimed;
	std::set<Node_Register*> registersToBeRetimed;
	determineAreaToBeRetimed(area, anchoredRegisters, output, areaToBeRetimed, registersToBeRetimed, ignoreRefs);


	{
		std::array<const BaseNode*,1> arr{output.node};
		ConstSubnet csub = ConstSubnet::allNecessaryForNodes({}, arr);
		csub.add(output.node);

		DotExport exp("DriversOfOutput.dot");
		exp(circuit, csub);
		exp.runGraphViz("DriversOfOutput.svg");
	}
	{
		ConstSubnet csub;
		for (auto n : areaToBeRetimed) csub.add(n);
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, csub);
		exp.runGraphViz("areaToBeRetimed.svg");
	}

	std::set<hlim::NodePort> outputsLeavingRetimingArea;
	// Find every output leaving the area
	for (auto n : areaToBeRetimed)
		for (auto i : utils::Range(n->getNumOutputPorts()))
			for (auto np : n->getDirectlyDriven(i))
				if (!areaToBeRetimed.contains(np.node)) {
					outputsLeavingRetimingArea.insert({.node = n, .port = i});
					break;
				}

	HCL_ASSERT(!registersToBeRetimed.empty());
	auto *clock = (*registersToBeRetimed.begin())->getClocks()[0];

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileProgram(circuit, {outputsLeavingRetimingArea});
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


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
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

	// Remove input registers that have now been retimed forward
	for (auto *reg : registersToBeRetimed)
		reg->bypassOutputToInput(0, (unsigned)Node_Register::DATA);
}



void retimeForward(Circuit &circuit, const Subnet &subnet)
{

}


}
