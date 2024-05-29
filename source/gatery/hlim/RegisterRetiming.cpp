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
#include "CNF.h"
#include "RevisitCheck.h"
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
#include "supportNodes/Node_RetimingBlocker.h"
#include "supportNodes/Node_NegativeRegister.h"
#include "supportNodes/Node_Attributes.h"
#include "supportNodes/Node_SignalTap.h"
#include "../utils/Enumerate.h"
#include "../utils/Zip.h"
#include <gatery/debug/DebugInterface.h>

#include "../simulation/ReferenceSimulator.h"

#include "postprocessing/MemoryDetector.h"


#include "../export/DotExport.h"

#include <sstream>
#include <iostream>
#include <optional>

//#ifdef _DEBUG
//# define DEBUG_OUTPUT
//#endif

namespace gtry::hlim {

Conjunction suggestForwardRetimingEnableCondition(Circuit &circuit, Subnet &area, NodePort output, bool ignoreRefs, Subnet *conjunctionArea)
{
	std::optional<Conjunction> enableCondition;

	std::vector<NodePort> openList;
	openList.push_back(output);

	RevisitCheck alreadyHandled(circuit);

	while (!openList.empty()) {
		auto nodePort = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (alreadyHandled.contains(nodePort.node)) continue;
		alreadyHandled.insert(nodePort.node);
		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(nodePort.node))
			continue;

		if (dynamic_cast<Node_RetimingBlocker*>(nodePort.node))
			continue;

		auto *regSpawner = dynamic_cast<Node_RegSpawner*>(nodePort.node);

		// We may not want to retime nodes to which references are still being held
		// References to node spawners are ok (?)
		if (nodePort.node->hasRef() && !ignoreRefs && !regSpawner)
			continue;

		// We can not retime nodes with a side effect
		if (nodePort.node->hasSideEffects())
			continue;

		// Check 
		bool isRegisterSource = false;
		if (regSpawner)
			isRegisterSource = true;
		else if (auto *reg = dynamic_cast<Node_Register*>(nodePort.node)) {
			if (reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_FORWARD)) {
				isRegisterSource = true;
			}
		}

		if (isRegisterSource) {
			for (size_t i : utils::Range(nodePort.node->getNumInputPorts())) {
				if (nodePort.node->inputIsEnable(i)) {
					Conjunction enableTerm;
					if (nodePort.node->getDriver(i).node != nullptr)
						enableTerm.parseInput({.node = nodePort.node, .port = i}, conjunctionArea);
					
					if (enableCondition)
						enableCondition->intersectTermsWith(enableTerm);
					else
						enableCondition = enableTerm;
				}
			}
		} else
		if (auto *negReg = dynamic_cast<Node_NegativeRegister*>(nodePort.node)) {  // Negative registers need their expected enables to be ignored
			for (size_t i : {(size_t)Node_NegativeRegister::Inputs::data}) {
				auto driver = negReg->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver);
			}
		} else {
			// Regular nodes just get added to the retiming area and their inputs are further explored
			for (size_t i : utils::Range(nodePort.node->getNumInputPorts())) {
				auto driver = nodePort.node->getDriver(i);
				// Only follow the data port as the enable port is special.
				if (!nodePort.node->inputIsEnable(i))
					if (driver.node != nullptr)
						if (driver.node->getOutputConnectionType(driver.port).type != ConnectionType::DEPENDENCY)
							openList.push_back(driver);
			}

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(nodePort.node)) { // If it is a memory port attempt to retime entire memory
				auto *memory = memPort->getMemory();

				if (!memory->getAttribs().arbitraryPortRetiming) {			
					// add all memory ports to open list
					for (auto np : memory->getPorts()) 
						openList.push_back(np);
				}
			}
		}
	}

	if (enableCondition)
		return *enableCondition;
	return {};
}




/**
 * @brief Manages the open list while simultaneously keeps track of how we got there.
 * @details This allows later on to generate subnets of the precise trace to a node for better error reporting.
 */
class TraceableOpenList {
	public:
		bool empty() const { return m_openList.empty(); }
		NodePort pop() {
			auto res = m_openList.back();
			m_openList.pop_back();
			return res;
		}
		void insert(NodePort nodePort, BaseNode* source) {
			m_openList.push_back(nodePort);
			if (source != nullptr)
				m_backTrace[nodePort.node].push_back(source);
		}

		Subnet traceBack(BaseNode* from) const {
			Subnet res;
			traceBack(res, from);
			return res;
		}
	protected:
		std::vector<NodePort> m_openList;
		utils::UnstableMap<BaseNode*, std::vector<BaseNode*>> m_backTrace;

		void traceBack(Subnet &subnet, BaseNode* from) const {
			if (subnet.contains(from)) return;
			subnet.add(from);

			auto it = m_backTrace.find(from);
			if (it == m_backTrace.end()) return;

			for (auto n : it->second)
				traceBack(subnet, n);
		}
};





struct ForwardRetimingPlan
{
	/// Area that will be retimed forward (excluding the registers).
	Subnet areaToBeRetimed;
	/// Registers that lead into areaToBeRetimed and which will have to be removed.
	utils::StableSet<Node_Register*> registersToBeRemoved;
	/// Register spawners that need to spawn registers which in turn can be retimed forward.
	utils::StableSet<Node_RegSpawner*> regSpawnersToSpawn;

	struct EnableReplacement {
		Subnet newNodes;
		NodePort input;
		NodePort newEnable;
	};

	/// Enables on anchored registers and memory write ports that need replacement
	std::vector<EnableReplacement> enableReplacements;

	/// Empty open list, only included to provide backtraces for error messages or warnings.
	TraceableOpenList openList;
};




namespace {

	void forwardPlanningHandleEnablePort(NodePort enableInput, 
							Conjunction portCondition, 
							const Conjunction &retimingEnableCondition, 
							Subnet &retimingArea, 
							ForwardRetimingPlan &retimingPlan,
							TraceableOpenList &openList) {

		if (enableInput.node->getDriver(enableInput.port).node == nullptr)
			return;

		// If there is an enable, we need to rebuild the subset of the reg's enable conjunction that does not include
		// the retiming enable conjunction and start retiming into that.
		// It needs to be duplicated since other parts of the circuit might depend on the original.

		portCondition.removeTerms(retimingEnableCondition);

		// Ensure that the contribution to regEnable are (or become part of) the retiming area
		for (const auto pair : portCondition.getTerms().anyOrder())
			retimingArea.add(pair.second.conjunctionDriver.node);


		NodeGroup *group = enableInput.node->getGroup();

		// Build the new enable logic. Here we break a bit with the paradigm of not modifying the 
		// graph in the planning phase, but if the retiming fails the nodes will be unused and can
		// be optimized away.
		// The new enable signal is added to a list to later on replace the old enable of the register
		// and is further explored for retiming.
		ForwardRetimingPlan::EnableReplacement repl;
		NodePort retimingEnablePart = portCondition.build(*group, &repl.newNodes);
		NodePort nonRetimingEnableCondition = retimingEnableCondition.build(*group, &repl.newNodes);
		
		// The full new enable condition is the global enable condition (unretimed) AND the retimed residual condition
		// If any of them are "empty" (unconnected defaults to TRUE in this case) then we don't need an AND node.
		if (retimingEnablePart.node != nullptr) {
			if (nonRetimingEnableCondition.node != nullptr) {
				Circuit &circuit = group->getCircuit();
				auto *andNode = circuit.createNode<Node_Logic>(Node_Logic::AND);
				repl.newNodes.add(andNode);
				andNode->moveToGroup(group);
				andNode->recordStackTrace();
				andNode->connectInput(0, retimingEnablePart);
				andNode->connectInput(1, nonRetimingEnableCondition);
				repl.newEnable = {.node = andNode, .port = 0ull};
				// The and node is still part of the retiming area to bridge from the main retiming area to the retimed residual condition
				retimingPlan.areaToBeRetimed.add(andNode);
			} else
				repl.newEnable = retimingEnablePart;
		} else
			repl.newEnable = nonRetimingEnableCondition;
		repl.input = enableInput;

		// Ensure that the new nodes of regEnable become part of the retiming area
		for (const auto node : repl.newNodes)
			retimingArea.add(node);

		if (retimingEnablePart.node != nullptr)
			openList.insert(retimingEnablePart, enableInput.node);
		retimingPlan.enableReplacements.push_back(std::move(repl));
	}

}



struct AddTraceAndWarnOfLatches
{
	TraceableOpenList &openList;
	BaseNode *node;
	Conjunction &enableCondition;
	const Subnet &enableConditionArea;
};

dbg::LogMessage &operator<<(dbg::LogMessage &msg, const Conjunction &conjunction)
{
	if (conjunction.getTerms().empty()) {
		return msg << "('1')";
	} else {
		msg << "(";
		bool first = true;
		for (const auto &term : conjunction.getTerms().anyOrder()) {
			if (!first) msg << " and "; else first = false;
			if (term.second.negated)
				msg << "not ";
			msg << term.second.conjunctionDriver;
		}
		msg << ")";
		return msg;
	}
}

dbg::LogMessage &operator<<(dbg::LogMessage &msg, const AddTraceAndWarnOfLatches &trc)
{
	Subnet subnet = trc.openList.traceBack(trc.node);

	for (auto n : subnet) {
		if (auto *sig = dynamic_cast<Node_Signal*>(n)) {
			if (n->getName() == "gtry_retiming_latch") {
				msg 
					<< "The signal passes through a register-turned-latch from a previous retiming run: " << sig
					<< ". This is usually an indicator that the retiming was attempted with the wrong enable condition, derived from"
					" encountering incompatible enable conditions. The enable condition is: " << trc.enableCondition << ". It was derived from " << trc.enableConditionArea << ". ";
			}
		}
	}
	
	subnet.dilate(true, true);

	msg << "Trace: " << std::move(subnet);

	return msg;
}


/**
 * @brief Determines the exact area to be forward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted. The area may increase if the planner needs to create new nodes.
 * @param output The output that shall receive a register.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined and if so the retiming plan
 */
std::optional<ForwardRetimingPlan> determineAreaToBeRetimedForward(Circuit &circuit, Subnet &area, NodePort output, Conjunction enableCondition, const Subnet &enableConditionArea, bool ignoreRefs = false, bool failureIsError = true)
{
	ForwardRetimingPlan retimingPlan;

	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	TraceableOpenList openList;
	openList.insert(output, nullptr);

	while (!openList.empty()) {

		auto nodePort = openList.pop();
		// Continue if the node was already encountered.
		if (retimingPlan.areaToBeRetimed.contains(nodePort.node)) continue;
		if (retimingPlan.registersToBeRemoved.contains((Node_Register*)nodePort.node)) continue;
		if (retimingPlan.regSpawnersToSpawn.contains((Node_RegSpawner*)nodePort.node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;

		if (dynamic_cast<Node_RetimingBlocker*>(nodePort.node)) continue;

		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(nodePort.node)) {
			if (!failureIsError) return {};

			dbg::log(dbg::LogMessage{} 
						<< dbg::LogMessage::LOG_ERROR
						<< dbg::LogMessage::LOG_POSTPROCESSING
						<< dbg::LogMessage::Anchor{ output.node->getGroup() }
						<< "An error occured attempting to retime forward to output " << output
						<< ": The fanning-in signals leave the specified operation area through node " << nodePort.node << " without passing a register that can be retimed forward."
						 << AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

			HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
		}

		auto *regSpawner = dynamic_cast<Node_RegSpawner*>(nodePort.node);

		// We may not want to retime nodes to which references are still being held
		// References to node spawners are ok (?)
		if (nodePort.node->hasRef() && !ignoreRefs && !regSpawner) {
			if (!failureIsError) return {};

			dbg::log(dbg::LogMessage{} 
						<< dbg::LogMessage::LOG_ERROR
						<< dbg::LogMessage::LOG_POSTPROCESSING
						<< dbg::LogMessage::Anchor{ output.node->getGroup() }
						<< "An error occured attempting to retime forward to output " << output
						<< ": The fanning-in signals are driven by a node to which references are still being held " << nodePort.node
						<< ". " << AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

			HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
		}			

		// Check that everything is using the same clock.
		for (auto *c : nodePort.node->getClocks()) {
			if (c != nullptr) {
				if (clock == nullptr) {
					clock = c;
					clockGivingNode = nodePort.node;
				} else {
					if (clock != c) {
						if (!failureIsError) return {};

						dbg::log(dbg::LogMessage{} 
									<< dbg::LogMessage::LOG_ERROR
									<< dbg::LogMessage::LOG_POSTPROCESSING
									<< dbg::LogMessage::Anchor{ output.node->getGroup() }
									<< "An error occured attempting to retime forward to output " << output
									<< ": The fanning-in signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode
									<< " and " << nodePort.node
									<< ". " << AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

						HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		if (nodePort.node->hasSideEffects()) {
			if (!failureIsError) return {};


			dbg::log(dbg::LogMessage{} 
						<< dbg::LogMessage::LOG_ERROR
						<< dbg::LogMessage::LOG_POSTPROCESSING
						<< dbg::LogMessage::Anchor{ output.node->getGroup() }
						<< "An error occured attempting to retime forward to output " << output
						<< ": The fanning-in signals are driven by a node with side effects " << nodePort.node
						<< " which can not be retimed. " << AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

			HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
		}		


		// Everything seems good with this node, so proceed
		if (regSpawner) {  // Register spawners spawn registers, so stop here
			retimingPlan.regSpawnersToSpawn.insert(regSpawner);

			// We do need to check for enable condition compatibility
			if (regSpawner->getDriver(Node_RegSpawner::INPUT_ENABLE).node != nullptr) {
				Conjunction spawnerEnable;
				Subnet spawnerEnableArea;
				spawnerEnable.parseInput({.node = regSpawner, .port = Node_RegSpawner::INPUT_ENABLE}, &spawnerEnableArea);

				if (!enableCondition.isSubsetOf(spawnerEnable)) {
					if (!failureIsError) return {};

					dbg::log(dbg::LogMessage{} 
								<< dbg::LogMessage::LOG_ERROR
								<< dbg::LogMessage::LOG_POSTPROCESSING
								<< dbg::LogMessage::Anchor{ output.node->getGroup() }
								<< "An error occured attempting to retime forward to output " << output
								<< ": The fanning-in signals are driven by a register spawner " << nodePort.node
								<< " with an enable signal that is incompatible with the inferred register enable signal of the retiming operation. "
								<< "The retiming enable is " << enableCondition << " derived from " << enableConditionArea
								<< ". The spawner's enable is " << spawnerEnable << " derived from " << spawnerEnableArea << ". "
								<< AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

					HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
				}
			}

		} else
		if (auto *reg = dynamic_cast<Node_Register*>(nodePort.node)) {  // Registers need special handling
			Conjunction regEnable;
			Subnet regEnableArea;

			// We do need to check for enable condition compatibility
			if (reg->getDriver(Node_Register::ENABLE).node != nullptr) {
				regEnable.parseInput({.node = reg, .port = Node_Register::ENABLE}, &regEnableArea);

				if (!enableCondition.isSubsetOf(regEnable)) {
					if (!failureIsError) return {};

					dbg::log(dbg::LogMessage{} 
								<< dbg::LogMessage::LOG_ERROR
								<< dbg::LogMessage::LOG_POSTPROCESSING
								<< dbg::LogMessage::Anchor{ output.node->getGroup() }
								<< "An error occured attempting to retime forward to output " << output
								<< ": The fanning-in signals are driven by a register " << nodePort.node
								<< " with an enable signal that is incompatible with the inferred register enable signal of the retiming operation. "  
								<< "The retiming enable is " << enableCondition << " derived from " << enableConditionArea
								<< ". The register's enable is " << regEnable << " derived from " << regEnableArea << ". "
								<< AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

					HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());				
				}
			}


			// Retime over anchored registers
			if (!reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_FORWARD)) {
				retimingPlan.areaToBeRetimed.add(nodePort.node);

				if (reg->getDriver(Node_Register::DATA).node != nullptr)
					openList.insert(reg->getDriver(Node_Register::DATA), reg);

				forwardPlanningHandleEnablePort(
					{.node = reg, .port = Node_Register::ENABLE},
					std::move(regEnable),
					enableCondition,
					area,
					retimingPlan,
					openList);

			} else {
				// Found a register to retime forward, stop here.
				retimingPlan.registersToBeRemoved.insert(reg);

				// It is important to not add the register to the area to be retimed!
				// If the register is part of a loop that is part of the retiming area, 
				// the input to the register effectively leaves the retiming area, thus forcing the placement
				// of a new register with a new reset value. The old register is bypassed thus
				// replacing the old register with the new register.
				// In other words, for registers that are completely embeddded in the retiming area,
				// this mechanism implicitely advances the reset value by one iteration which is necessary 
				// because we can not retime a register out of the reset-pin-path.

				// Don't build and retime into a modified enable condition as with anchored registers.
				// No retiming into the enable condition is needed and the modification/splitting of the 
				// enable condition is done when implementing the plan.
			}
		} else
		if (auto *negReg = dynamic_cast<Node_NegativeRegister*>(nodePort.node)) {  // Negative Registers need special handling
			Conjunction regEnable;
			Subnet regEnableArea;

			// We do need to check for enable condition compatibility
			if (negReg->expectedEnable().node != nullptr) {
				regEnable.parseInput({.node = negReg, .port = (size_t) Node_NegativeRegister::Inputs::expectedEnable}, &regEnableArea);

				if (!enableCondition.isSubsetOf(regEnable)) {
					if (!failureIsError) return {};

					dbg::log(dbg::LogMessage{} 
								<< dbg::LogMessage::LOG_ERROR
								<< dbg::LogMessage::LOG_POSTPROCESSING
								<< dbg::LogMessage::Anchor{ output.node->getGroup() }
								<< "An error occured attempting to retime forward to output " << output
								<< ": The fanning-in signals are driven by a negative register " << nodePort.node
								<< " with an enable signal that is incompatible with the inferred register enable signal of the retiming operation. "
								<< "The retiming enable is " << enableCondition << " derived from " << enableConditionArea
								<< ". The negative register's enable is " << regEnable << " derived from " << regEnableArea << ". "
								<< AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

					HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
				}
			}

			retimingPlan.areaToBeRetimed.add(nodePort.node);

			if (negReg->getDriver((size_t) Node_NegativeRegister::Inputs::data).node != nullptr)
				openList.insert(negReg->getDriver((size_t) Node_NegativeRegister::Inputs::data), negReg);

			forwardPlanningHandleEnablePort(
				{.node = negReg, .port = (size_t) Node_NegativeRegister::Inputs::expectedEnable},
				std::move(regEnable),
				enableCondition,
				area,
				retimingPlan,
				openList);

		} else if (auto *memPort = dynamic_cast<Node_MemPort*>(nodePort.node)) { // If it is a memory port attempt to retime entire memory
			auto *memory = memPort->getMemory();
			retimingPlan.areaToBeRetimed.add(memory);
		
			if (!memory->getAttribs().arbitraryPortRetiming) {			
				// add all memory ports to open list
				for (auto np : memory->getPorts()) {
					HCL_ASSERT(np.node->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr);
					openList.insert(np, memPort);
				}
			}

			HCL_ASSERT(memPort->getDriver((size_t)Node_MemPort::Inputs::enable).node == nullptr);

			// We do need to check for enable condition compatibility
			if (memPort->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr) {
				Conjunction portEnable;
				Subnet portEnableArea;
				portEnable.parseInput({.node = memPort, .port = (size_t)Node_MemPort::Inputs::wrEnable}, &portEnableArea);

				if (!enableCondition.isSubsetOf(portEnable)) {
					if (!failureIsError) return {};

					dbg::log(dbg::LogMessage{} 
								<< dbg::LogMessage::LOG_ERROR
								<< dbg::LogMessage::LOG_POSTPROCESSING
								<< dbg::LogMessage::Anchor{ output.node->getGroup() }
								<< "An error occured attempting to retime forward to output " << output
								<< ": The retiming area contains a memory write port " << nodePort.node
								<< " with an enable signal that is incompatible with the inferred register enable signal of the retiming operation. "
								<< "The retiming enable is " << enableCondition << " derived from " << enableConditionArea
								<< ". The port's enable is " << portEnable << " derived from " << portEnableArea << ". "
								<< AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

					HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());				
				}

				forwardPlanningHandleEnablePort(
					{.node = memPort, .port = (size_t)Node_MemPort::Inputs::wrEnable},
					std::move(portEnable),
					enableCondition,
					area,
					retimingPlan,
					openList);				
			}

			retimingPlan.areaToBeRetimed.add(memPort);
			for (auto i : {Node_MemPort::Inputs::address, Node_MemPort::Inputs::wrData, Node_MemPort::Inputs::wrWordEnable}) {
				auto driver = nodePort.node->getDriver((size_t)i);
				if (driver.node != nullptr)
					openList.insert(driver, memPort);
			}
		} else {
			// Regular nodes just get added to the retiming area and their inputs are further explored
			retimingPlan.areaToBeRetimed.add(nodePort.node);
			for (size_t i : utils::Range(nodePort.node->getNumInputPorts())) {
				auto driver = nodePort.node->getDriver(i);
				if (driver.node != nullptr) {

					if (nodePort.node->inputIsEnable(i)) {

						Conjunction enable;
						Subnet enableArea;
						enable.parseInput({.node = nodePort.node, .port = i}, &enableArea);

						if (!enableCondition.isSubsetOf(enable)) {
							if (!failureIsError) return {};


							dbg::log(dbg::LogMessage{} 
										<< dbg::LogMessage::LOG_ERROR
										<< dbg::LogMessage::LOG_POSTPROCESSING
										<< dbg::LogMessage::Anchor{ output.node->getGroup() }
										<< "An error occured attempting to retime forward to output " << output
										<< ": The fanning-in signals are driven by a node " << nodePort.node
										<< " with an enable signal that is incompatible with the inferred register enable signal of the retiming operation. "
										<< "The retiming enable is " << enableCondition << " derived from " << enableConditionArea
										<< ". The node's enable is " << enable << " derived from " << enableArea << ". "
										<< AddTraceAndWarnOfLatches{ openList, nodePort.node, enableCondition, enableConditionArea });

							HCL_DESIGNCHECK_HINT(false, "A retiming error occured, check the log for details: " + dbg::howToReachLog());
						}

						forwardPlanningHandleEnablePort(
							{.node = nodePort.node, .port = i},
							std::move(enable),
							enableCondition,
							area,
							retimingPlan,
							openList);							
					} else 
						if (driver.node->getOutputConnectionType(driver.port).type != ConnectionType::DEPENDENCY)
							openList.insert(driver, nodePort.node);
				}
			}
		}
	}

	retimingPlan.openList = std::move(openList);

	return retimingPlan;
}

NodePort buildHoldingCircuit(NodePort driver, NodePort enable, NodePort resetValue, Clock *clock, NodeGroup &group, Subnet &area)
{
	Circuit &circuit = group.getCircuit();

	auto *sig1 = circuit.appendSignal(enable);
	sig1->setName("latch_passthrough");
	area.add(sig1);

	auto *mux = circuit.createNode<Node_Multiplexer>(2);
	mux->recordStackTrace();
	area.add(mux);
	mux->moveToGroup(&group);
	mux->connectSelector(enable);
	mux->connectInput(1, driver);


	auto *reg = circuit.createNode<Node_Register>();
	reg->recordStackTrace();
	area.add(reg);
	reg->moveToGroup(&group);
	reg->setClock(clock);
	reg->connectInput(Node_Register::DATA, {.node = mux, .port = 0ull});
	reg->connectInput(Node_Register::RESET_VALUE, resetValue);

	mux->connectInput(0, {.node = reg, .port = 0ull});

	NodePort output = {.node = mux, .port = 0ull};

	auto *sig2 = circuit.appendSignal(output);
	// This signal node, recognized by name, is also used later on (if encountered) to trigger diagnostics messages about this whole trick only working once.
	sig2->setName("gtry_retiming_latch");
	sig2->setComment("A register with an enable signal was forward retimed with an enable condition that is"
					 " not fully equal. The \"residual\" enable condition must be handled by a holding circuit"
					 " that is a combinatorical pass-through but can \"latch\" that signal for when the enable is deasserted.");
	area.add(sig2);

	return output;
}


bool retimeForwardToOutput(Circuit &circuit, Subnet &area, NodePort output, const RetimingSetting &settings)
{
	// Track as well for better reporting
	Subnet enableConditionArea;
	Conjunction enableCondition = suggestForwardRetimingEnableCondition(circuit, area, output, settings.ignoreRefs, &enableConditionArea);

	HCL_ASSERT(!enableCondition.isUndefined());
	HCL_ASSERT(!enableCondition.isContradicting());

	auto retimingPlan = determineAreaToBeRetimedForward(circuit, area, output, enableCondition, enableConditionArea, settings.ignoreRefs, settings.failureIsError);

	if (!retimingPlan)
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

	utils::StableSet<hlim::NodePort> outputsLeavingRetimingArea;
	// Find every output leaving the area
	for (auto n : retimingPlan->areaToBeRetimed)
		for (auto i : utils::Range(n->getNumOutputPorts()))
			if (n->getOutputConnectionType(i).type != ConnectionType::DEPENDENCY)
				for (auto np : n->getDirectlyDriven(i)) {

					// this is a bit of a quick hack, stuff driven by the Node_RetimingBlocker should not be part of the retiming area in the first place.
					if (dynamic_cast<Node_RetimingBlocker*>(np.node->getNonSignalDriver(np.port).node))
						continue;

					if (!retimingPlan->areaToBeRetimed.contains(np.node)) {
						outputsLeavingRetimingArea.insert({.node = n, .port = i});
						break;
					}
				}

	if (retimingPlan->regSpawnersToSpawn.size() > 1) {

		Subnet joinedTraces;
		for (auto spawner : retimingPlan->regSpawnersToSpawn)
			joinedTraces.add(retimingPlan->openList.traceBack(spawner));

		joinedTraces.dilate(true, true);

		dbg::LogMessage logMsg{};
		logMsg << dbg::LogMessage::LOG_WARNING
					<< dbg::LogMessage::LOG_POSTPROCESSING
					<< dbg::LogMessage::Anchor{ output.node->getGroup() }
					<< "Registers for retiming to a single location are sourced from " << retimingPlan->regSpawnersToSpawn.size()
					<< "different register spawners. This is usually a mistake. Register Spawners: ";

		for (auto spawner : retimingPlan->regSpawnersToSpawn)
			logMsg << spawner;

		logMsg << ". Traces: " << joinedTraces;
		dbg::log(logMsg);

		std::cout << "Warning in register retiming, check the log for details: " << dbg::howToReachLog() << std::endl;
	}

	Subnet newNodes;

	auto registersToCheckForBypass = retimingPlan->registersToBeRemoved;

	// Spawn register spawners
	for (auto *spawner : retimingPlan->regSpawnersToSpawn) {
		auto regs = spawner->spawnForward();
		// Add new regs to area subnet to keep that up to date
		for (auto r : regs) {
			newNodes.add(r);
			registersToCheckForBypass.insert(r);
		}
	}

	if (registersToCheckForBypass.empty()) // no registers found to retime, probably everything is constant, so no clock available
		return false;

	HCL_ASSERT(!registersToCheckForBypass.empty());
	auto *clock = (*registersToCheckForBypass.begin())->getClocks()[0];

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
	sim::SimulatorCallbacks ignoreCallbacks;
	sim::ReferenceSimulator simulator(false);
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
		// If we have an enable condition, build the logic for it in the same node group and connect to the register enable
		reg->connectInput(Node_Register::ENABLE, enableCondition.build(*np.node->getGroup(), &newNodes));

		// allow further retiming by default
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

		if (settings.downstreamDisableForwardRT) {
			bool isDownstream = combinatoricallyDrivenArea.contains(np.node) || (np == output);

			if (!isDownstream)
				reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		} else {
			reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		}
		
		newNodes.add(reg);

		// If any input bit is defined upon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).type);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			newNodes.add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}

		// Find all signals leaving the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (!retimingPlan->areaToBeRetimed.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}
	

	// Replace enables of anchored registers and memory write ports
	for (const auto &repl : retimingPlan->enableReplacements) {
		repl.input.node->rewireInput(repl.input.port, repl.newEnable);
		for (auto &n : repl.newNodes)
			newNodes.add(n);
	}



	utils::UnstableMap<NodePort, NodePort> residualEnables;

	std::vector<std::pair<NodePort, NodePort>> bypasses;
	// Bypass input registers for the retimed nodes
	// When bypassing, check for enable condition compatibility.
	// Note, we need to check once and then apply the bypasses.
	// If this is not done in two separate steps, inputs might
	// gobble up multiple registers in a row.
	for (auto *reg : registersToCheckForBypass) {

		NodePort residualEnable;
		// Build the "residual enable" unless a cached version already exists
		if (reg->getDriver(Node_Register::ENABLE).node != nullptr) {
			auto it = residualEnables.find(reg->getDriver(Node_Register::ENABLE));
			if (it != residualEnables.end())
				residualEnable = it->second;
			else {
				Conjunction regEnable;
				regEnable.parseInput({.node=reg, .port=Node_Register::ENABLE});
				regEnable.removeTerms(enableCondition);
				residualEnable = regEnable.build(*reg->getGroup(), &newNodes);
				residualEnables[reg->getDriver(Node_Register::ENABLE)] = residualEnable;
			}
		}

		NodePort driver = reg->getDriver((unsigned)Node_Register::DATA);

		// If there is some form of enable remaining, the register can not simply be bypassed.
		// Instead, a holding circuit needs to be implemented.
		if (residualEnable.node != nullptr)
			driver = buildHoldingCircuit(reg->getDriver(Node_Register::DATA), 
								reg->getDriver(Node_Register::ENABLE),//residualEnable, 
								reg->getDriver(Node_Register::RESET_VALUE),
								clock,
								*reg->getGroup(),
								newNodes);

		// Finally, rewire everything that goes from the register into the retiming area to use the register's
		// driver or, potentially, the holding circuit.
		// Note, that this needs to be delayed until all bypasses have been determined as an input might
		// consume multiple registers.
		for (auto driven : reg->getDirectlyDriven(0))
			if (retimingPlan->areaToBeRetimed.contains(driven.node))
				bypasses.push_back({driven, driver});
	}

	// After everything has been prepared, actually apply the bypasses.
	for (const auto &driven_newDriver : bypasses)
		driven_newDriver.first.node->rewireInput(driven_newDriver.first.port, driven_newDriver.second);
	
	

	for (auto &n : newNodes)
		area.add(n);

	if (settings.newNodes) 
		*settings.newNodes = std::move(newNodes);

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












Conjunction suggestBackwardRetimingEnableCondition(Circuit &circuit, Subnet &area, NodePort output, const utils::StableSet<Node_MemPort*> &retimeableWritePorts, bool ignoreRefs)
{
	std::vector<NodePort> openList;
	for (auto np : output.node->getDirectlyDriven(output.port))
		openList.push_back(np);


	RevisitCheck alreadyHandled(circuit);

	while (!openList.empty()) {
		auto nodePort = openList.back();
		auto *node = nodePort.node;
		openList.pop_back();
		// Continue if the node was already encountered.
		if (alreadyHandled.contains(node)) continue;
		alreadyHandled.insert(node);

		if (!area.contains(node)) 
			continue;
	
		// We may not want to retime nodes to which references are still being held
		if (node->hasRef() && !ignoreRefs)
			continue;


		// We can not retime nodes with a side effect
		if (node->hasSideEffects())
			continue;

		// Everything seems good with this node, so proceeed
		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling

			if (nodePort.port != (size_t)Node_Register::DATA)
				continue;


			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_BACKWARD)) {
				Conjunction enableTerm;
				if (reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr)
					enableTerm.parseInput({.node = reg, .port = Node_Register::ENABLE});
				
				return enableTerm;
			} else {
				for (size_t i : utils::Range(node->getNumOutputPorts()))
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np);
			}
		} else {
			// Regular nodes just get added to the retiming area and their outputs are further explored
			for (size_t i : utils::Range(node->getNumOutputPorts()))
				if (node->getOutputConnectionType(i).type != ConnectionType::DEPENDENCY)
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
					// add all memory ports to open list
					for (auto np : memory->getPorts()) 
						openList.push_back(np);
				}
			 }
		}
	}

	return {};
}





struct BackwardRetimingPlan
{
	/// Area that will be retimed forward (excluding the registers).
	Subnet areaToBeRetimed;
	/// Registers that lead into areaToBeRetimed and which will have to be removed.
	utils::StableSet<Node_Register*> registersToBeRemoved;

	struct EnableReplacement {
		Subnet newNodes;
		NodePort input;
		NodePort newEnable;
	};

	/// Enables on anchored registers and memory write ports that need replacement
	std::vector<EnableReplacement> enableReplacements;
	/// Inputs into the retiming area that shall not be delayed
	utils::UnstableSet<NodePort> undelayedInputs;
	/// Inputs into the retiming area that shall be delayed but are enables to be reset to zero
	utils::StableSet<NodePort> delayedInputs;
};



namespace {

	void backwardPlanningHandleEnablePort(NodePort enableInput, 
							NodeGroup *group,
							Conjunction portCondition, 
							const Conjunction &retimingEnableCondition, 
							Subnet &retimingArea, 
							BackwardRetimingPlan &retimingPlan) {

		// If there is an enable, we need to rebuild the subset of the reg's enable conjunction that does not include
		// the retiming enable conjunction and start retiming into that.
		// It needs to be duplicated since other parts of the circuit might depend on the original.

		portCondition.removeTerms(retimingEnableCondition);

		// Ensure that the contribution to regEnable are (or become part of) the retiming area
		for (const auto pair : portCondition.getTerms().anyOrder())
			retimingArea.add(pair.second.conjunctionDriver.node);


		Circuit &circuit = group->getCircuit();

		// Build the new enable logic. Here we break a bit with the paradigm of not modifying the 
		// graph in the planning phase, but if the retiming fails the nodes will be unused and can
		// be optimized away.
		// The new enable signal is added to a list to later on replace the old enable of the register
		// and is further explored for retiming.
		BackwardRetimingPlan::EnableReplacement repl;
		NodePort retimingEnablePart = portCondition.build(*group, &repl.newNodes);
		NodePort nonRetimingEnableCondition = retimingEnableCondition.build(*group, &repl.newNodes);
		
		// Enforce at least a constant one retimable part so that there is a branch onto which the retiming
		// can put a register with a reset value of zero.
		if (retimingEnablePart.node == nullptr) {
			Node_Constant *constOne = circuit.createNode<Node_Constant>(true);
			constOne->recordStackTrace();
			constOne->moveToGroup(group);
			retimingEnablePart = {.node = constOne, .port = 0ull};
		}


		// The full new enable condition is the global enable condition (unretimed) AND the retimed residual condition
		// If any of them are "empty" (unconnected defaults to TRUE in this case) then we don't need an AND node.
		if (nonRetimingEnableCondition.node != nullptr) {
			auto *andNode = circuit.createNode<Node_Logic>(Node_Logic::AND);
			repl.newNodes.add(andNode);
			andNode->moveToGroup(group);
			andNode->recordStackTrace();
			andNode->connectInput(0, retimingEnablePart);
			retimingPlan.delayedInputs.insert({.node=andNode, .port=0ull});
			andNode->connectInput(1, nonRetimingEnableCondition);
			retimingPlan.undelayedInputs.insert({.node=andNode, .port=1ull});
			repl.newEnable = {.node = andNode, .port = 0ull};
			// The and node is still part of the retiming area to bridge from the main retiming area to the retimed residual condition
			retimingPlan.areaToBeRetimed.add(andNode);
		} else {
			repl.newEnable = retimingEnablePart;
			retimingPlan.delayedInputs.insert(enableInput);
		}
		repl.input = enableInput;

		

		// Ensure that the new nodes of regEnable become part of the retiming area
		for (const auto node : repl.newNodes)
			retimingArea.add(node);

		retimingPlan.enableReplacements.push_back(std::move(repl));
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
std::optional<BackwardRetimingPlan> determineAreaToBeRetimedBackward(Circuit &circuit, Subnet &area, NodePort output, const utils::StableSet<Node_MemPort*> &retimeableWritePorts, Conjunction enableCondition, bool ignoreRefs = false, bool failureIsError = true)
{
	BackwardRetimingPlan retimingPlan;

	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<NodePort> openList;
	for (auto np : output.node->getDirectlyDriven(output.port))
		openList.push_back(np);


#ifdef DEBUG_OUTPUT
	auto writeSubnet = [&]{
		{
			DotExport exp("areaToBeRetimed.dot");
			exp(circuit, retimingPlan.areaToBeRetimed.asConst());
			exp.runGraphViz("areaToBeRetimed.svg");	
		}
		{
			ConstSubnet net;
			net.dilate(DilateDir::output, 1, output.node);
			net.dilateIf<Node_Register, Node_MemPort>(DilateDir::none, DilateDir::output);
			net.dilateIf<Node_Register>(DilateDir::input, DilateDir::none, 1);

			DotExport exp("retiming_registers.dot");
			exp(circuit, net);
			exp.runGraphViz("retiming_registers.svg");
		}
		{
			Subnet subnet = retimingPlan.areaToBeRetimed;
			//subnet.dilate(false, true);
			//subnet.dilate(false, true);
			subnet.dilate(false, true);
			subnet.dilate(true, true);

			DotExport exp("retiming_area.dot");
			exp(circuit, subnet.asConst());
			exp.runGraphViz("retiming_area.svg");
		}
		{
			DotExport exp("all.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("all.svg");	
		}
	};
#endif

	while (!openList.empty()) {
		auto nodePort = openList.back();
		auto *node = nodePort.node;
		openList.pop_back();
		// Continue if the node was already encountered.
		if (retimingPlan.areaToBeRetimed.contains(node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			if (!failureIsError) return {};

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
			if (!failureIsError) return {};

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
						if (!failureIsError) return {};

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
		if (node->hasSideEffects() 
			&& !dynamic_cast<Node_Attributes*>(node) && !dynamic_cast<Node_SignalTap*>(node)) { // Attrib hotfix
			if (!failureIsError) return {};

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
				if (!failureIsError) return {};
#ifdef DEBUG_OUTPUT
writeSubnet();
#endif
				std::stringstream error;

				error 
					<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
					<< "Node from:\n" << output.node->getStackTrace() << "\n";

				error 
					<< "The fanning-out signals are driving a non-data port of a register.\n	Register: " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
					<< "	From:\n" << node->getStackTrace() << "\n";

				HCL_ASSERT_HINT(false, error.str());
			}

			if (retimingPlan.registersToBeRemoved.contains(reg)) continue;

			Conjunction regEnable;

			// We do need to check for enable condition compatibility
			if (reg->getDriver(Node_Register::ENABLE).node != nullptr)
				regEnable.parseInput({.node = reg, .port = Node_Register::ENABLE});


			if (!enableCondition.isSubsetOf(regEnable)) {
				if (!failureIsError) return {};

#ifdef DEBUG_OUTPUT
writeSubnet();
#endif
				std::stringstream error;

				error 
					<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
					<< "Node from:\n" << output.node->getStackTrace() << "\n";

				error 
					<< "The fanning-out signals are driving a register " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ", id " << nodePort.node->getId()
					<< ") with an enable signal that is incompatible with the inferred register enable signal of the retiming operation.\n"
					<< "Register from:\n" << nodePort.node->getStackTrace() << "\n";

				HCL_ASSERT_HINT(false, error.str());
			}


			// Retime over anchored registers and registers with incompatible enable conditions.
			if (!reg->getFlags().contains(Node_Register::Flags::ALLOW_RETIMING_BACKWARD) || !enableCondition.isEqualTo(regEnable)) {

				// Retime over this register.
				retimingPlan.areaToBeRetimed.add(node);
				for (size_t i : utils::Range(node->getNumOutputPorts()))
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np);

				backwardPlanningHandleEnablePort(
					{.node = reg, .port = Node_Register::ENABLE},
					reg->getGroup(),
					std::move(regEnable),
					enableCondition,
					area,
					retimingPlan);						
			} else {
				// Found a register to retime backward, stop here.
				retimingPlan.registersToBeRemoved.insert(reg);

				//registerEnable = reg->getNonSignalDriver(Node_Register::ENABLE);
				//enableGivingRegister = reg;

				// It is important to not add the register to the area to be retimed!
				// If the register is part of a loop that is part of the retiming area, 
				// the output of the register effectively leaves the retiming area, thus forcing the placement
				// of a new register and a reset value check.
			}
		} else {
			// Regular nodes just get added to the retiming area and their outputs are further explored
			retimingPlan.areaToBeRetimed.add(node);
			for (size_t i : utils::Range(node->getNumOutputPorts()))
				if (node->getOutputConnectionType(i).type != ConnectionType::DEPENDENCY)
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np);

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port		 	

				// We do need to check for enable condition compatibility
				Conjunction portEnable;
				if (memPort->getDriver((size_t)Node_MemPort::Inputs::wrEnable).node != nullptr)
					portEnable.parseInput({.node = memPort, .port = (size_t)Node_MemPort::Inputs::wrEnable});

				if (!enableCondition.isSubsetOf(portEnable)) {
					if (!failureIsError) return {};

		#ifdef DEBUG_OUTPUT
			writeSubnet();
		#endif
					std::stringstream error;

					error 
						<< "An error occurred attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
						<< "Node from:\n" << output.node->getStackTrace() << "\n";

					error 
						<< "The retiming area contains a memory write port " << nodePort.node->getName() << " (" << nodePort.node->getTypeName() << ", id " << nodePort.node->getId()
						<< ") with an enable signal that is incompatible with the inferred register enable signal of the retiming operation.\n"
						<< "Memory write port from:\n" << nodePort.node->getStackTrace() << "\n";

					HCL_ASSERT_HINT(false, error.str());					
				}

				auto *group = memPort->getGroup();
				while (group->getGroupType() == NodeGroupType::SFU)
					group = group->getParent();

				backwardPlanningHandleEnablePort(
					{.node = memPort, .port = (size_t)Node_MemPort::Inputs::wrEnable},
					group,
					std::move(portEnable),
					enableCondition,
					area,
					retimingPlan);				

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
					retimingPlan.areaToBeRetimed.add(memory);
				
					// add all memory ports to open list
					for (auto np : memory->getPorts()) 
						openList.push_back(np);
				}
			 }
		}
	}

	for (const auto &term : enableCondition.getTerms().anyOrder()) {
		if (retimingPlan.areaToBeRetimed.contains(term.first.node)) {
			if (!failureIsError) return {};

#ifdef DEBUG_OUTPUT
writeSubnet();
#endif
			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals are driving register with enable signals that are driven from within the area that is to be retimed.\n"
				<< "	Node: " << term.first.node->getName() << " (" << term.first.node->getTypeName() << ", id " << term.first.node->getId() << ").\n"
				<< "	From:\n" << term.first.node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}
	}

	return retimingPlan;
}

bool retimeBackwardtoOutput(Circuit &circuit, Subnet &area, const utils::StableSet<Node_MemPort*> &retimeableWritePorts,
							std::optional<Conjunction> requiredEnableCondition,
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

	Conjunction enableCondition;
	if (requiredEnableCondition)
		enableCondition = *requiredEnableCondition;
	else
		enableCondition = suggestBackwardRetimingEnableCondition(circuit, area, output, retimeableWritePorts, ignoreRefs);

	HCL_ASSERT(!enableCondition.isUndefined());
	HCL_ASSERT(!enableCondition.isContradicting());

	auto retimingPlan = determineAreaToBeRetimedBackward(circuit, area, output, retimeableWritePorts, enableCondition, ignoreRefs, failureIsError);

	if (!retimingPlan)
		return false;

	retimedArea = retimingPlan->areaToBeRetimed;
/*
	{
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, retimedArea.asConst());
		exp.runGraphViz("areaToBeRetimed.svg");
	}
*/
	if (retimedArea.empty()) return true; // immediately hit a register, so empty retiming area, nothing to do.

	utils::StableSet<hlim::NodePort> outputsEnteringRetimingArea;
	// Find every output entering the area
	for (auto n : retimedArea)
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node != nullptr && !outputIsDependency(driver) && !retimedArea.contains(driver.node))
				outputsEnteringRetimingArea.insert(driver);
		}

	utils::StableSet<hlim::NodePort> outputsLeavingRetimingArea;
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
	if (!retimingPlan->registersToBeRemoved.empty()) {
		clock = (*retimingPlan->registersToBeRemoved.begin())->getClocks()[0];
	} else {
		// No register, this must be a RMW loop, find a write port and grab clock from there.
		for (auto wp : retimeableWritePorts) 
			if (retimedArea.contains(wp)) {
				clock = wp->getClocks()[0];
				break;
			}
	}

	HCL_ASSERT(clock != nullptr);

	// Run a simulation to determine the reset values of the registers that will be removed
	// and build logic to potentially override (fix) signals.
	/// @todo Clone and optimize to prevent issues with loops
	sim::SimulatorCallbacks ignoreCallbacks;
	sim::ReferenceSimulator simulator(false);
	simulator.compileProgram(circuit, {outputsLeavingRetimingArea}, true);
	simulator.powerOn();

	utils::UnstableMap<std::tuple<Clock*, NodeGroup*, NodePort>, NodePort> delayedResetSignals;
	// Get a signal that is zero during reset and in the first non-reset cycle. It turns one after unless the enable signal is held low.
	auto getDelayedResetSignalFor = [&](Clock *clk, NodeGroup *grp, NodePort enable)->NodePort {
		auto it = delayedResetSignals.find({clk, grp, enable});
		if (it != delayedResetSignals.end()) 
			return it->second;

		Node_Constant *constZeroOne[2];
		for (auto i = 0; i < 2; i++) {
			constZeroOne[i] = circuit.createNode<Node_Constant>(i != 0);
			constZeroOne[i]->recordStackTrace();
			constZeroOne[i]->moveToGroup(grp);
			area.add(constZeroOne[i]);
			if (newNodes) newNodes->add(constZeroOne[i]);
		}

		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clk);
		// Setup to switch from zero to one
		reg->connectInput(Node_Register::DATA, {.node = constZeroOne[1], .port = 0ull});
		reg->connectInput(Node_Register::RESET_VALUE, {.node = constZeroOne[0], .port = 0ull});
		// Connect to same enable as all the register it is replacing
		reg->connectInput(Node_Register::ENABLE, enable);
		reg->moveToGroup(grp);
		reg->setComment("Use a register to create a reset signal that is delayed by one cycle. I.e. it is zero during reset and for one cycle after, but then becomes and stays one (unless an enable is held low).");
		
		area.add(reg);
		if (newNodes) newNodes->add(reg);

		delayedResetSignals[{clk, grp, enable}] = {.node = reg, .port = 0ull};
		return {.node = reg, .port = 0ull};
	};

	for (auto reg : retimingPlan->registersToBeRemoved) {

		auto resetDriver = reg->getNonSignalDriver(Node_Register::RESET_VALUE);
		if (resetDriver.node != nullptr) {
			auto resetValue = evaluateStatically(circuit, resetDriver);
			auto inputValue = simulator.getValueOfOutput(reg->getDriver(0));

			HCL_ASSERT(resetValue.size() == inputValue.size());
			
			if (!sim::canBeReplacedWith(resetValue, inputValue)) {

				auto delayedResetSignal = getDelayedResetSignalFor(reg->getClocks()[0], reg->getGroup(), reg->getDriver(Node_Register::ENABLE));

				auto *mux = circuit.createNode<Node_Multiplexer>(2);
				mux->recordStackTrace();


				mux->connectSelector(delayedResetSignal);
				mux->connectInput(0, resetDriver);
				mux->connectInput(1, {.node = reg, .port = 0ull});
				mux->moveToGroup(reg->getGroup());
				mux->setComment("A register with a reset value was retimed backwards from here. To preserve the reset value, this multiplexer overrides the signal during reset and in the first cycle after with the original reset value.");

				std::vector<NodePort> driven = reg->getDirectlyDriven(0);
				for (auto inputNP : driven)
					if (inputNP.node != mux)
						inputNP.node->rewireInput(inputNP.port, {.node = mux, .port = 0ull});

				area.add(mux);
				if (newNodes) newNodes->add(mux);
			}
		}
	}


	Subnet newlyCreatedNodes;


	for (const auto &repl : retimingPlan->enableReplacements) {
		repl.input.node->rewireInput(repl.input.port, repl.newEnable);
		for (auto n : repl.newNodes)
			newlyCreatedNodes.add(n);
	}

	// Insert regular registers
	for (auto np : outputsEnteringRetimingArea) {

		NodeGroup *nodeGroup = np.node->getGroup();
		if (nodeGroup->getGroupType() == NodeGroupType::SFU)
			nodeGroup = nodeGroup->getParent();	

		// Don't insert registers on constants
		if (dynamic_cast<Node_Constant*>(np.node)) continue;
		// Don't insert registers on signals leading to constants
		if (auto *signal = dynamic_cast<Node_Signal*>(np.node))
			if (dynamic_cast<Node_Constant*>(signal->getNonSignalDriver(0).node)) continue;

		NodePort enableSignal = enableCondition.build(*nodeGroup, &newlyCreatedNodes);

		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clock);
		// Setup input data
		reg->connectInput(Node_Register::DATA, np);
		// Connect to same enable as all the removed registers
		reg->connectInput(Node_Register::ENABLE, enableSignal);
		// add to the node group of its new driver
		reg->moveToGroup(nodeGroup);
		// allow further retiming by default
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
		reg->setComment("This register was created during backwards retiming as one of the registers on signals going into the retimed area.");
		
		newlyCreatedNodes.add(reg);

		// If any input bit is defined upon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).type);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			newlyCreatedNodes.add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}


		// Find all signals entering the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (retimedArea.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			if (!retimingPlan->undelayedInputs.contains(inputNP) && !retimingPlan->delayedInputs.contains(inputNP))
				inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}

	utils::UnstableMap<NodePort, NodePort> enablePortRegCache;

	// Insert registers on enable ports
	// These are special in that they always reset to zero.
	for (auto input : retimingPlan->delayedInputs) {

		auto driver = input.node->getDriver(input.port);

		if (retimedArea.contains(driver.node))
			continue;

		NodeGroup *nodeGroup = input.node->getGroup();
		if (nodeGroup->getGroupType() == NodeGroupType::SFU)
			nodeGroup = nodeGroup->getParent();


		auto it = enablePortRegCache.find(driver);
		if (it != enablePortRegCache.end()) {
			input.node->rewireInput(input.port, it->second);
		} else {
			NodePort enableSignal = enableCondition.build(*nodeGroup, &newlyCreatedNodes);

			auto *reg = circuit.createNode<Node_Register>();
			reg->recordStackTrace();
			// Setup clock
			reg->setClock(clock);
			// Setup input data
			reg->connectInput(Node_Register::DATA, driver);
			// Connect to same enable as all the removed registers
			reg->connectInput(Node_Register::ENABLE, enableSignal);
			// add to the node group of its new driver
			reg->moveToGroup(nodeGroup);
			// allow further retiming by default
			reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);
			newlyCreatedNodes.add(reg);

			// Force reset to zero
			auto *resetZero = circuit.createNode<Node_Constant>(false);
			resetZero->recordStackTrace();
			resetZero->moveToGroup(reg->getGroup());
			newlyCreatedNodes.add(resetZero);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetZero, .port = 0ull});

			NodePort newDriver = {.node = reg, .port = 0ull};
			input.node->rewireInput(input.port, newDriver);
			enablePortRegCache[driver] = newDriver;
		}
	}


	for (auto n : newlyCreatedNodes) {
		area.add(n);
		if (newNodes) newNodes->add(n);
	}

	// Remove output registers that have now been retimed forward
	for (auto *reg : retimingPlan->registersToBeRemoved)
		reg->bypassOutputToInput(0, (unsigned)Node_Register::DATA);

/*
	{
			DotExport exp("after_backward_retiming.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("after_backward_retiming.svg");	
	}
*/
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
	utils::UnstableMap<NodePort, sim::DefaultBitVectorState> resetValues;
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
			rdPortAddrShiftReg[i] = createRegister(rdPortAddrShiftReg[i-1], {}, rdPort.enableInputDriver);


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
					wordSignals[wordIdx].conflict = createRegister(wordSignals[wordIdx].conflict, {}, rdPort.enableInputDriver);
					wordSignals[wordIdx].overrideData = createRegister(wordSignals[wordIdx].overrideData, {}, rdPort.enableInputDriver);
					wordSignals[wordIdx].overrideWpIdx = createRegister(wordSignals[wordIdx].overrideWpIdx, {}, rdPort.enableInputDriver);
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
				conflict = createRegister(conflict, {}, rdPort.enableInputDriver);
				overrideData = createRegister(overrideData, {}, rdPort.enableInputDriver);
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



void ReadModifyWriteHazardLogicBuilder::determineResetValues(utils::UnstableMap<NodePort, sim::DefaultBitVectorState> &resetValues)
{
	utils::StableSet<NodePort> requiredNodePorts;

	for (auto &p : resetValues.anyOrder())
		if (p.first.node != nullptr)
			requiredNodePorts.insert(p.first);

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
	sim::SimulatorCallbacks ignoreCallbacks;
	sim::ReferenceSimulator simulator(false);
	simulator.compileStaticEvaluation(m_circuit, requiredNodePorts);
	simulator.powerOn();

	for (auto &p : resetValues.anyOrder())
		if (p.first.node != nullptr)
			p.second = simulator.getValueOfOutput(p.first);
}

NodePort ReadModifyWriteHazardLogicBuilder::createRegister(NodePort nodePort, const sim::DefaultBitVectorState &resetValue, NodePort enable)
{
	if (nodePort.node == nullptr)
		return {};

	auto *reg = m_circuit.createNode<Node_Register>();
	reg->moveToGroup(m_newNodesNodeGroup);

	reg->recordStackTrace();
	reg->setClock(m_clockDomain);
	reg->connectInput(Node_Register::DATA, nodePort);
	reg->connectInput(Node_Register::ENABLE, enable);
	reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD).insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);

	// If any input bit is defined upon reset, add that as a reset value
	if (sim::anyDefined(resetValue, 0, resetValue.size())) {
		auto *resetConst = m_circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(nodePort).type);
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
		rewireNode->changeOutputType({.type = ConnectionType::BOOL, .width=1});
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

