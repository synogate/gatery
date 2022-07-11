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
#include "Circuit.h"

#include "../debug/DebugInterface.h"

#include "Node.h"
#include "NodeIO.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Pin.h"
#include "coreNodes/Node_Rewire.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Compare.h"

#include "supportNodes/Node_Attributes.h"

#include "postprocessing/MemoryDetector.h"
#include "postprocessing/DefaultValueResolution.h"
#include "postprocessing/AttributeFusion.h"
#include "postprocessing/TechnologyMapping.h"
#include "postprocessing/Retiming.h"
#include "postprocessing/CDCDetection.h"


#include "../simulation/BitVectorState.h"
#include "../simulation/ReferenceSimulator.h"
#include "../utils/Range.h"

#include "Subnet.h"


#include <set>
#include <vector>
#include <map>

#include <iostream>

namespace gtry::hlim {

Circuit::Circuit()
{
	m_root.reset(new NodeGroup(*this, NodeGroup::GroupType::ENTITY));
}

/**
 * @brief Copies a subnet of all nodes that are needed to drive the specified outputs up to the specified inputs
 * @note Do note that subnetInputs specified input ports, not output ports!
 * @param source The circuit to copy from
 * @param subnetInputs Input ports where to stop copying
 * @param subnetOutputs Output ports from where to start copying.
 * @param mapSrc2Dst Map from all copied nodes in the source circuit to the corresponding node in the destination circuit.
 */
void Circuit::copySubnet(const std::set<NodePort> &subnetInputs,
						 const std::set<NodePort> &subnetOutputs,
						 std::map<BaseNode*, BaseNode*> &mapSrc2Dst,
						 bool copyClocks)
{
	mapSrc2Dst.clear();

	std::set<BaseNode*> closedList;
	std::vector<NodePort> openList = std::vector<NodePort>(subnetOutputs.begin(), subnetOutputs.end());

	std::vector<std::pair<std::uint64_t, BaseNode*>> sortedNodes;

	// Scan and copy unconnected nodes
	while (!openList.empty()) {
		NodePort nodePort = openList.back();
		openList.pop_back();
		if (closedList.contains(nodePort.node)) continue;
		closedList.insert(nodePort.node);

		BaseNode *newNode = createUnconnectedClone(nodePort.node);
		mapSrc2Dst[nodePort.node] = newNode;
		sortedNodes.push_back({nodePort.node->getId(), newNode});
		for (auto i : utils::Range(nodePort.node->getNumInputPorts())) {
			auto driver = nodePort.node->getDriver(i);
			if (driver.node == nullptr) continue;
			if (subnetInputs.contains({.node=nodePort.node, .port=i})) continue;
			openList.push_back(driver);
		}
	}

	// Sort new nodes based on old ids to give them new ids (unique to this circuit) while preserving the order
	std::sort(sortedNodes.begin(), sortedNodes.end());
	for (auto [oldId, node] : sortedNodes)
		node->setId(m_nextNodeId++, {});

	// Reestablish connections, create clock network on demand
	std::map<Clock*, Clock*> mapSrc2Dst_clocks;
	std::function<Clock*(Clock*)> lazyCreateClockNetwork;

	lazyCreateClockNetwork = [&](Clock *oldClock)->Clock*{
		auto it = mapSrc2Dst_clocks.find(oldClock);
		if (it != mapSrc2Dst_clocks.end()) return it->second;

		Clock *oldParent = oldClock->getParentClock();
		Clock *newParent = nullptr;
		if (oldParent != nullptr)
			newParent = lazyCreateClockNetwork(oldParent);
		return createUnconnectedClock(oldClock, newParent);
	};


	for (auto oldNew : mapSrc2Dst) {
		auto oldNode = oldNew.first;
		auto newNode = oldNew.second;
		for (auto i : utils::Range(oldNode->getNumInputPorts())) {
			auto driver = oldNode->getDriver(i);
			if (driver.node == nullptr) continue;
			auto it = mapSrc2Dst.find(driver.node);
			if (it != mapSrc2Dst.end())
				newNode->rewireInput(i, {.node = it->second, .port=driver.port});
		}

		for (auto i : utils::Range(oldNode->getClocks().size())) {
			auto *oldClock = oldNode->getClocks()[i];
			if (oldClock != nullptr)
				if (copyClocks)
					newNode->attachClock(lazyCreateClockNetwork(oldClock), i);
				else
					newNode->attachClock(oldClock, i);
		}
	}
}


BaseNode *Circuit::createUnconnectedClone(BaseNode *srcNode, bool noId)
{
	m_nodes.push_back(srcNode->cloneUnconnected());
	if (!noId)
		m_nodes.back()->setId(m_nextNodeId++, {});
	return m_nodes.back().get();
}


Clock *Circuit::createUnconnectedClock(Clock *clock, Clock *newParent)
{
	m_clocks.push_back(clock->cloneUnconnected(newParent));
	return m_clocks.back().get();
}

/**
 * @brief Infers signal names for all unnamed signals
 *
 */
void Circuit::inferSignalNames()
{
	std::set<Node_Signal*> unnamedSignals;
	for (size_t i = 0; i < m_nodes.size(); i++)
		if (auto *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get()))
			if (signal->getName().empty()) {
				auto driver = signal->getDriver(0);
				if (driver.node != nullptr) {
					auto name = driver.node->attemptInferOutputName(driver.port);
					if (!name.empty()) {
						if (name.size() > 200)
							signal->setInferredName("unnamed");
						else
							signal->setInferredName(std::move(name));
					}
				} else 
					unnamedSignals.insert(signal);
			}


	while (!unnamedSignals.empty()) {

		Node_Signal *s = *unnamedSignals.begin();
		std::vector<Node_Signal*> signalsToName;
		std::set<Node_Signal*> loopDetection;

		signalsToName.push_back(s);
		loopDetection.insert(s);

		for (auto nh : s->exploreInput(0).skipDependencies()) {
			if (nh.isSignal()) {
				if (loopDetection.contains((Node_Signal*)nh.node())) {
					nh.node()->setInferredName("loop");
					nh.backtrack();
				} else
					if (!nh.node()->getName().empty()) {
						nh.backtrack();
					} else {
						signalsToName.push_back((Node_Signal*)nh.node());
						loopDetection.insert((Node_Signal*)nh.node());
					}
			}
		}

		for (auto it = signalsToName.rbegin(); it != signalsToName.rend(); ++it) {
			if ((*it)->getName().empty()) {
				auto driver = (*it)->getDriver(0);
				if (driver.node != nullptr) {
					auto name = driver.node->attemptInferOutputName(driver.port);
					if (!name.empty()) {
						if (name.size() > 200)
							(*it)->setInferredName("unnamed");
						else
							(*it)->setInferredName(std::move(name));
					}
				} else
					(*it)->setInferredName("undefined");
			}
			unnamedSignals.erase(*it);
		}
	}
}


void Circuit::disconnectZeroBitSignalNodes()
{
	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
		if (signal == nullptr)
			continue;

		if (signal->getOutputConnectionType(0).width == 0) {
			signal->disconnectInput();
		}
	}
}

void Circuit::disconnectZeroBitOutputPins()
{
	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Pin *pin = dynamic_cast<Node_Pin*>(m_nodes[i].get());
		if (pin == nullptr)
			continue;

		if (pin->isOutputPin() && pin->getConnectionType().width == 0) {
			dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing output pin " << pin << " because it's output is of width zero");
			pin->disconnect();
		}
	}
}

void Circuit::optimizeRewireNodes(Subnet &subnet)
{
	for (auto &n : subnet)
		if (Node_Rewire *rewireNode = dynamic_cast<Node_Rewire*>(n))
			rewireNode->optimize();
}

/**
 * @brief Removes unnecessary and unnamed signal nodes
 *
 */
void Circuit::cullUnnamedSignalNodes()
{
	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
		if (signal == nullptr)
			continue;

		// Don't cull named signals
		if (!signal->getName().empty())
			continue;

		// Don't cull signals with comments
		if (!signal->getComment().empty())
			continue;

		//TODO don't cull signals with attributes

		// Don't cull signals that are still referenced by frontend objects
		if (signal->hasRef()) continue;

		// We want to keep one signal node between non-signal nodes, so only cull if the input or all outputs are signals.
		bool inputIsSignalOrUnconnected = (signal->getDriver(0).node == nullptr) || (dynamic_cast<Node_Signal*>(signal->getDriver(0).node) != nullptr);

		bool allOutputsAreSignals = true;
		for (const auto &c : signal->getDirectlyDriven(0)) {
			allOutputsAreSignals &= (dynamic_cast<Node_Signal*>(c.node) != nullptr);
		}

		if (inputIsSignalOrUnconnected || allOutputsAreSignals) {
			if (signal->getDriver(0).node != signal)
				signal->bypassOutputToInput(0, 0);
			signal->disconnectInput();

			if (i+1 != m_nodes.size())
				m_nodes[i] = std::move(m_nodes.back());
			m_nodes.pop_back();
			i--;
		}
	}
}

/**
 * @brief Removes signal nodes that are unnecessary because there is a sequence of identical ones
 *
 */
void Circuit::cullSequentiallyDuplicatedSignalNodes()
{
	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
		if (signal == nullptr)
			continue;

		// Don't cull signals that are still referenced by frontend objects
		if (signal->hasRef()) continue;

		auto driver = signal->getDriver(0);

		if (auto *driverSignal = dynamic_cast<Node_Signal*>(driver.node)) {

			if (driverSignal->getName() == signal->getName() &&
				driverSignal->getComment() == signal->getComment() &&
				driverSignal->getGroup() == signal->getGroup() &&
				driverSignal->getSignalGroup() == signal->getSignalGroup()) {
				// todo: Check attributes

				if (driverSignal == signal) continue;
				
				signal->bypassOutputToInput(0, 0);
				signal->disconnectInput();

				if (i+1 != m_nodes.size())
					m_nodes[i] = std::move(m_nodes.back());
				m_nodes.pop_back();
				i--;
			}
		}
	}
}

/**
 * @brief Cull signal nodes without inputs or outputs
 *
 */
void Circuit::cullOrphanedSignalNodes()
{
	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
		if (signal == nullptr)
			continue;

		// Don't cull signals that are still referenced by frontend objects
		if (signal->hasRef()) continue;

		if (signal->isOrphaned()) {
			if (i+1 != m_nodes.size())
				m_nodes[i] = std::move(m_nodes.back());
			m_nodes.pop_back();
			i--;
		}
	}
}

void Circuit::cullUnusedNodes(Subnet &subnet)
{
	// Do multiple times since some nodes (memory write ports) might loose their side effects when others vanish
	bool done;
	do {
		done = true;

		auto usedNodes = Subnet::allUsedNodes(*this);

		// intersect usedNudes with subnet

		// Remove everything that isn't used
		for (size_t i = 0; i < m_nodes.size(); i++) {
			if (!usedNodes.getNodes().contains(m_nodes[i].get()) && subnet.contains(m_nodes[i].get())) {
				subnet.remove(m_nodes[i].get());
				if (i+1 != m_nodes.size())
					m_nodes[i] = std::move(m_nodes.back());
				m_nodes.pop_back();
				i--;
				done = false;
			}
		}
	} while (!done);
}






struct HierarchyCondition {
	std::map<NodePort, bool> m_conditionsAndNegations;
	bool m_undefined = false;
	bool m_contradicting = false;

	void parse(NodePort nodeInput) {
		std::vector<std::pair<NodePort, bool>> stack;
		if (nodeInput.node != nullptr)
			stack.push_back({nodeInput.node->getNonSignalDriver(nodeInput.port), false});
		else
			m_undefined = true;

		while (!stack.empty()) {
			auto top = stack.back();
			stack.pop_back();
			if (top.first.node == nullptr)
				m_undefined = true;
			else {
				if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(top.first.node)) {
					if (logicNode->getOp() == Node_Logic::NOT) {
						auto driver = logicNode->getNonSignalDriver(0);
						stack.push_back({driver, !top.second});
					} else
					if (logicNode->getOp() == Node_Logic::AND) {
						for (auto j : utils::Range(logicNode->getNumInputPorts()))
							stack.push_back({logicNode->getNonSignalDriver(j), top.second});
					} else {
						auto it = m_conditionsAndNegations.find(top.first);
						if (it != m_conditionsAndNegations.end())
							m_contradicting |= it->second != top.second;
						else
							m_conditionsAndNegations[top.first] = top.second;
					}
				} else {
					auto it = m_conditionsAndNegations.find(top.first);
					if (it != m_conditionsAndNegations.end())
						m_contradicting |= it->second != top.second;
					else
						m_conditionsAndNegations[top.first] = top.second;
				}
			}
		}
	}

	bool isEqualOf(const HierarchyCondition &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return true;

		if (m_conditionsAndNegations.size() != other.m_conditionsAndNegations.size()) return false;
		for (const auto &pair : m_conditionsAndNegations) {
			auto it = other.m_conditionsAndNegations.find(pair.first);
			if (it == other.m_conditionsAndNegations.end()) return false;
			if (it->second != pair.second) return false;
		}
		return true;
	}

	bool isNegationOf(const HierarchyCondition &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return false;

		if (m_conditionsAndNegations.size() != other.m_conditionsAndNegations.size()) return false;
		for (const auto &pair : m_conditionsAndNegations) {
			auto it = other.m_conditionsAndNegations.find(pair.first);
			if (it == other.m_conditionsAndNegations.end()) return false;
			if (it->second == pair.second) return false;
		}
		return true;
	}

	bool isSubsetOf(const HierarchyCondition &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return false;

		for (const auto &pair : m_conditionsAndNegations) {
			auto it = other.m_conditionsAndNegations.find(pair.first);
			if (it == other.m_conditionsAndNegations.end()) return false;
			if (it->second != pair.second) return false;
		}
		return true;
	}
};

void Circuit::mergeMuxes(Subnet &subnet)
{
	bool done;
	do {
		done = true;

		for (size_t i = 0; i < m_nodes.size(); i++) {
			if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
				if (muxNode->getNumInputPorts() != 3) continue;

				//std::cout << "Found 2-input mux" << std::endl;

				HierarchyCondition condition;
				condition.parse({.node = muxNode, .port = 0});

				for (size_t muxInput : utils::Range(2)) {

					auto input0 = muxNode->getNonSignalDriver(muxInput?2:1);
					auto input1 = muxNode->getNonSignalDriver(muxInput?1:2);

					if (input1.node == nullptr)
						continue;

					if (Node_Multiplexer *prevMuxNode = dynamic_cast<Node_Multiplexer*>(input0.node)) {
						if (prevMuxNode == muxNode) continue; // sad thing
						if (!subnet.contains(prevMuxNode)) continue; // todo: Optimize

						//std::cout << "Found 2 chained muxes" << std::endl;

						HierarchyCondition prevCondition;
						prevCondition.parse({.node = prevMuxNode, .port = 0});

						bool conditionsMatch = false;
						bool prevConditionNegated;

						if (prevCondition.isEqualOf(condition)) {
							conditionsMatch = true;
							prevConditionNegated = muxInput==1;
						} else if (condition.isNegationOf(prevCondition)) {
							conditionsMatch = true;
							prevConditionNegated = muxInput==0;
						} else {
							/*
							std::cout << "Condition 1 is :" << std::endl;
							if (condition.m_undefined) std::cout << "   undefined" << std::endl;
							if (condition.m_contradicting) std::cout << "   contradicting" << std::endl;
							std::cout << "	";
							for (auto p : condition.m_conditionsAndNegations) {
								std::cout << " and ";
								if (p.second)
									std::cout << "not ";
								std::cout << std::hex << p.first.node << ':' << p.first.port;
							}
							std::cout << std::endl;
							std::cout << "Condition 2 is :" << std::endl;
							if (prevCondition.m_undefined) std::cout << "   undefined" << std::endl;
							if (prevCondition.m_contradicting) std::cout << "   contradicting" << std::endl;
							std::cout << "	";
							for (auto p : prevCondition.m_conditionsAndNegations) {
								std::cout << " and ";
								if (p.second)
									std::cout << "not ";
								std::cout << std::hex << p.first.node << ':' << p.first.port;
							}
							std::cout << std::endl;
							*/
						}

						if (conditionsMatch) {
							//std::cout << "Conditions match!" << std::endl;
							dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Merging muxes " << prevMuxNode << " and " << muxNode << " because they form an if then else pair");

							auto bypass = prevMuxNode->getDriver(prevConditionNegated?2:1);
							// Connect second mux directly to bypass
							muxNode->connectInput(muxInput, bypass);

							done = false;
						}
					}
				}
			}
		}
	} while (!done);
}


void Circuit::mergeRewires(Subnet &subnet)
{
	bool done;
	do {
		done = true;

		for (size_t i = 0; i < m_nodes.size(); i++) {
			if (Node_Rewire *rewireNode = dynamic_cast<Node_Rewire*>(m_nodes[i].get())) {

				for (size_t inputIdx : utils::Range(rewireNode->getNumInputPorts())) {

					auto input = rewireNode->getNonSignalDriver(inputIdx);

					if (input.node == nullptr)
						continue;

					if (Node_Rewire *prevRewireNode = dynamic_cast<Node_Rewire*>(input.node)) {
						if (prevRewireNode == rewireNode) continue; // sad thing
						if (!subnet.contains(prevRewireNode)) continue; // todo: Optimize

						if (prevRewireNode->getGroup() != rewireNode->getGroup()) continue;

						if (prevRewireNode->getOp().ranges.size() == 1) { // keep it simple for now

							if (prevRewireNode->getOp().ranges[0].source == Node_Rewire::OutputRange::INPUT) {
								auto prevInputIdx = prevRewireNode->getOp().ranges[0].inputIdx;
								auto prevInputOffset = prevRewireNode->getOp().ranges[0].inputOffset;

								dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Merging rewires " << prevRewireNode << " and " << rewireNode << " by directly fetching from input " << prevInputIdx << " at bit offset " << prevInputOffset);

								auto op = rewireNode->getOp();
								for (auto &r : op.ranges) {
									if (r.source == Node_Rewire::OutputRange::INPUT && r.inputIdx == inputIdx)
										r.inputOffset += prevInputOffset;
								}
								rewireNode->setOp(std::move(op));

								rewireNode->connectInput(inputIdx, prevRewireNode->getDriver(prevInputIdx));
								done = false;
							}
						}
					}
				}
				
			}
		}
	} while (!done);
}

void Circuit::removeIrrelevantMuxes(Subnet &subnet)
{
	bool done;
	do {
		done = true;

		for (auto n : subnet) {
			if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(n)) {
				if (muxNode->getNumInputPorts() != 3) continue;

				//std::cout << "Found 2-input mux" << std::endl;

				HierarchyCondition condition;
				condition.parse({.node = muxNode, .port = 0});

				for (size_t muxInputPort : utils::Range(1,3)) {

					for (auto muxOutput : muxNode->getDirectlyDriven(0)) {
						std::vector<NodePort> openList = { muxOutput };
						std::set<NodePort> closedList;

						bool allSubnetOutputsMuxed = true;

						while (!openList.empty()) {
							NodePort input = openList.back();
							openList.pop_back();
							if (closedList.contains(input)) continue;
							closedList.insert(input);

							if (input.node->hasSideEffects() || input.node->hasRef() || !input.node->isCombinatorial()) {
								allSubnetOutputsMuxed = false;
								//std::cout << "Internal node with sideeffects, skipping" << std::endl;
								break;
							}

							if (input.node->getGroup() != muxNode->getGroup()) {
								allSubnetOutputsMuxed = false;
								//std::cout << "Internal node driving external, skipping" << std::endl;
								break;
							}

							if (Node_Multiplexer *subnetOutputMuxNode = dynamic_cast<Node_Multiplexer*>(input.node)) {
								if (muxNode->getNumInputPorts() == 3) {
									HierarchyCondition subnetOutputMuxNodeCondition;
									subnetOutputMuxNodeCondition.parse({.node = subnetOutputMuxNode, .port = 0});

									if (input.port == muxInputPort && condition.isEqualOf(subnetOutputMuxNodeCondition))
										continue;
									if (input.port != muxInputPort && condition.isNegationOf(subnetOutputMuxNodeCondition))
										continue;
								}
							}

							for (auto j : utils::Range(input.node->getNumOutputPorts()))
								for (auto driven : input.node->getDirectlyDriven(j))
									openList.push_back(driven);

						}

						if (allSubnetOutputsMuxed) {
							dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because only input " << muxInputPort << " is relevant further on");
							//std::cout << "Rewiring past mux" << std::endl;
							muxOutput.node->connectInput(muxOutput.port, muxNode->getDriver(muxInputPort));
							done = false;
						} else {
							//std::cout << "Not rewiring past mux" << std::endl;
						}
					}
				}
			}
		}
	} while (!done);
}

void Circuit::removeIrrelevantComparisons(Subnet &subnet)
{
	for (auto n : subnet) {
		if (auto *compNode = dynamic_cast<Node_Compare*>(n)) {
			auto leftDriver = compNode->getNonSignalDriver(0);
			auto rightDriver = compNode->getNonSignalDriver(1);

			if (compNode->getOp() != Node_Compare::EQ && compNode->getOp() != Node_Compare::NEQ) continue;
			if (leftDriver.node == nullptr || rightDriver.node == nullptr) continue;
			if (getOutputWidth(leftDriver) != 1) continue;
			if (getOutputConnectionType(leftDriver).interpretation != ConnectionType::BOOL ||
				getOutputConnectionType(rightDriver).interpretation != ConnectionType::BOOL)
				continue;

			Node_Constant *constInputs[2];
			constInputs[0] = dynamic_cast<Node_Constant*>(leftDriver.node);
			constInputs[1] = dynamic_cast<Node_Constant*>(rightDriver.node);

			for (auto i : utils::Range(2)) {
				if (constInputs[i] == nullptr) continue;
				if (constInputs[i]->getValue().get(sim::DefaultConfig::DEFINED, 0) == false) {
					dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
									<< " is comparing to an undefined signal coming from " << constInputs[i] << ". Hardwireing output to undefined source " << compNode->getDriver(i).node);
					// replace with undefined
					compNode->bypassOutputToInput(0, i);
					break;
				}
				bool invert = constInputs[i]->getValue().get(sim::DefaultConfig::VALUE, 0) ^ (compNode->getOp() == Node_Compare::EQ);
				if (invert) {
					// replace with inverter
					auto *notNode = createNode<Node_Logic>(Node_Logic::NOT);

					dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
									<< " is comparing to a constant and actually an inverter. Replacing with logic node for inversion " << notNode);

					notNode->moveToGroup(compNode->getGroup());
					notNode->setComment(notNode->getComment());
					notNode->recordStackTrace();
					notNode->connectInput(0, compNode->getDriver(i^1));
					subnet.add(notNode);

					auto driven = compNode->getDirectlyDriven(0);
					for (auto &d : driven)
						d.node->rewireInput(d.port, {.node=notNode, .port=0ull});

					break;
				} else {
					dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
							<< " is comparing to a constant and actually an identity function. Removing by directly attaching outputs to " << compNode->getDriver(i^1).node);
					// bypass
					compNode->bypassOutputToInput(0, i^1);
					break;
				}
			}
		}
	}
}


void Circuit::cullMuxConditionNegations(Subnet &subnet)
{
	for (auto n : subnet) {
		if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(n)) {
			if (muxNode->getNumInputPorts() != 3) continue;

			bool done;
			do {
				done = true;
				auto condition = muxNode->getNonSignalDriver(0);

				if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(condition.node)) {
					if (logicNode->getOp() == Node_Logic::NOT) {
						muxNode->connectSelector(logicNode->getDriver(0));

						auto input0 = muxNode->getDriver(1);
						auto input1 = muxNode->getDriver(2);

						muxNode->connectInput(0, input1);
						muxNode->connectInput(1, input0);

						dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing/bypassing negation " << logicNode << " to mux " << muxNode << " selector by swapping mux inputs.");

						done = false; // check same mux again to unravel chain of NOTs
					}
				}
			} while (!done);
		}
	}
}

/**
 * @details So far only removes no-op rewire nodes since they prevent block-ram detection
 *
 */
void Circuit::removeNoOps(Subnet &subnet)
{
	for (unsigned i = 0; i < m_nodes.size(); i++) {
		if (!subnet.contains(m_nodes[i].get())) continue;
		bool removeNode = false;

		if (auto *rewire = dynamic_cast<Node_Rewire*>(m_nodes[i].get())) {
			if (rewire->isNoOp()) {
				dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing rewire " << rewire << " because it is a no-op.");
				rewire->bypassOutputToInput(0, 0);
				removeNode = true;
			}
		}

		if (removeNode && !m_nodes[i]->hasRef()) {
			subnet.remove(m_nodes[i].get());
			if (i+1 != m_nodes.size())
				m_nodes[i] = std::move(m_nodes.back());
			m_nodes.pop_back();
			i--;
		}
	}
}


void Circuit::foldRegisterMuxEnableLoops(Subnet &subnet)
{
	std::vector<BaseNode*> newNodes;
	for (auto n : subnet) {
		if (auto *regNode = dynamic_cast<Node_Register*>(n)) {
			auto enableCondition = regNode->getNonSignalDriver((unsigned)Node_Register::Input::ENABLE);

			auto data = regNode->getNonSignalDriver((unsigned)Node_Register::Input::DATA);
			if (auto *muxNode = dynamic_cast<Node_Multiplexer*>(data.node)) {
				if (muxNode->getNumInputPorts() == 3) {

					auto muxInput1 = muxNode->getNonSignalDriver(1);
					auto muxInput2 = muxNode->getNonSignalDriver(2);

					auto muxCondition = muxNode->getDriver(0);

					if (muxInput1.node == regNode) {
						if (enableCondition.node != nullptr) {
							auto *andNode = createNode<Node_Logic>(Node_Logic::AND);
							newNodes.push_back(andNode);
							andNode->recordStackTrace();
							andNode->moveToGroup(regNode->getGroup());
							andNode->connectInput(0, enableCondition);
							andNode->connectInput(1, muxCondition);

							regNode->connectInput(Node_Register::Input::ENABLE, {.node=andNode, .port=0ull});
						} else {
							regNode->connectInput(Node_Register::Input::ENABLE, muxCondition);
						}
						regNode->connectInput(Node_Register::Input::DATA, muxNode->getDriver(2));
					} else if (muxInput2.node == regNode) {
						auto *notNode = createNode<Node_Logic>(Node_Logic::NOT);
						newNodes.push_back(notNode);
						notNode->recordStackTrace();
						notNode->moveToGroup(regNode->getGroup());
						notNode->connectInput(0, muxCondition);

						if (enableCondition.node != nullptr) {
							auto *andNode = createNode<Node_Logic>(Node_Logic::AND);
							newNodes.push_back(andNode);
							andNode->recordStackTrace();
							andNode->moveToGroup(regNode->getGroup());
							andNode->connectInput(0, enableCondition);
							andNode->connectInput(1, {.node=notNode,.port=0ull});

							regNode->connectInput(Node_Register::Input::ENABLE, {.node=andNode, .port=0ull});
						} else {
							regNode->connectInput(Node_Register::Input::ENABLE, {.node=notNode, .port=0ull});
						}
						regNode->connectInput(Node_Register::Input::DATA, muxNode->getDriver(1));
					}
				}
			}
		}
	}

	for (auto n : newNodes)
		subnet.add(n);
}

void Circuit::removeConstSelectMuxes(Subnet &subnet)
{
	for (auto n : subnet) {
		if (auto *muxNode = dynamic_cast<Node_Multiplexer*>(n)) {
			auto sel = muxNode->getNonSignalDriver(0);
			if (auto *constNode = dynamic_cast<Node_Constant*>(sel.node)) {
				if (constNode->getValue().size() == 0)
				{
					dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because its selector is constant, empty, and defaults to zero.");
					muxNode->bypassOutputToInput(0, 1);
				}
				else
				{
					HCL_ASSERT(constNode->getValue().size() < 64);
					std::uint64_t selDefined = constNode->getValue().extractNonStraddling(sim::DefaultConfig::DEFINED, 0, constNode->getValue().size());
					std::uint64_t selValue = constNode->getValue().extractNonStraddling(sim::DefaultConfig::VALUE, 0, constNode->getValue().size());
					if ((selDefined ^ (~0ull >> (64 - constNode->getValue().size()))) == 0) {
						dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because its selector is constant and defined.");
						muxNode->bypassOutputToInput(0, 1+selValue);
					}
				}
			}
		}
	}
}

void Circuit::propagateConstants(Subnet &subnet)
{
	//std::cout << "propagateConstants()" << std::endl;
	sim::SimulatorCallbacks ignoreCallbacks;

	std::vector<NodePort> openList;
	// std::set<NodePort> closedList;

	// Start walking the graph from the const nodes
	for (auto n : subnet) {
		if (Node_Constant *constNode = dynamic_cast<Node_Constant*>(n)) {
			openList.push_back({.node = constNode, .port = 0});
		}
	}

	while (!openList.empty()) {
		NodePort constPort = openList.back();
		openList.pop_back();
		/*
		if (closedList.contains(constPort)) continue;
		closedList.insert(constPort);
		*/

		// The output constPort is constant, loop over all nodes driven by this and look for nodes that can be computed
		auto& nodeList = constPort.node->getDirectlyDriven(constPort.port);
		for(size_t i = 0; i < nodeList.size(); ++i)
		{
			NodePort successor = nodeList[i];

			if (!subnet.contains(successor.node)) continue;

			// Signal nodes don't do anything, so add the output of the signal node to the openList
			if (Node_Signal *signalNode = dynamic_cast<Node_Signal*>(successor.node)) {
				openList.push_back({.node = signalNode, .port = 0});
				continue;
			}

			// Remove registers without reset or whose reset is compatible with the constant input
			if (auto *regNode = dynamic_cast<Node_Register*>(successor.node)) {
				bool bypassRegister = false;

				auto dataDriver = regNode->getNonSignalDriver((unsigned)Node_Register::Input::DATA);
				auto resetDriver = regNode->getNonSignalDriver((unsigned)Node_Register::Input::RESET_VALUE);
				auto *constNode = dynamic_cast<Node_Constant*>(dataDriver.node);

				// Only bypass if input driven by constant node
				if (constNode != nullptr) {
					if (resetDriver.node == nullptr)
						bypassRegister = true;
					else if (dataDriver.node != nullptr) {
						// evaluate reset value. Note that it is ok to only evaluate the reset value (it need not be constant) because the register only evaluates it during the reset.
						sim::ReferenceSimulator simulator;
						simulator.compileProgram(*this, {resetDriver}, true);
						simulator.powerOn();

						auto resetValue = simulator.getValueOfOutput(resetDriver);
						if (sim::canBeReplacedWith(resetValue, constNode->getValue())) 
							bypassRegister = true;
					}
				}

				if (bypassRegister) {
					regNode->bypassOutputToInput(0, (unsigned)Node_Register::Input::DATA);
					continue;
				} 
			}

			// Only work on combinatory nodes
			if (!successor.node->isCombinatorial()) continue;
			// Nodes with side-effects can't be removed/bypassed
			if (successor.node->hasSideEffects()) continue;
			// Nodes with references can't be removed/bypassed
			if (successor.node->hasRef()) continue;

			if (!successor.node->getInternalStateSizes().empty()) continue; // can't be good for const propagation

			// Attempt to compute the output of this node.
			// Build a state vector with all inputs. Set all non-const inputs to undefined.
			sim::DefaultBitVectorState state;
			std::vector<size_t> inputOffsets(successor.node->getNumInputPorts(), ~0ull);
			for (size_t port : utils::Range(successor.node->getNumInputPorts())) {
				auto driver = successor.node->getNonSignalDriver(port);
				if (driver.node != nullptr) {
					auto conType = hlim::getOutputConnectionType(driver);
					size_t offset = state.size();
					state.resize(offset + (conType.width + 63)/64 * 64);
					inputOffsets[port] = offset;
					if (Node_Constant *constNode = dynamic_cast<Node_Constant*>(driver.node)) {
						size_t arr[] = {offset};
						constNode->simulatePowerOn(ignoreCallbacks, state, nullptr, arr); // import const data
					} else {
						state.clearRange(sim::DefaultConfig::DEFINED, offset, conType.width);   // set non-const data to undefined
					}
				}
			}

			// Allocate Outputs
			std::vector<size_t> outputOffsets(successor.node->getNumOutputPorts(), ~0ull);
			for (size_t port : utils::Range(successor.node->getNumOutputPorts())) {
				auto conType = successor.node->getOutputConnectionType(port);
				size_t offset = state.size();
				state.resize(offset + (conType.width + 63)/64 * 64);
				outputOffsets[port] = offset;
			}

			// Simulate node
			successor.node->simulateEvaluate(ignoreCallbacks, state, nullptr, inputOffsets.data(), outputOffsets.data()); // compute outputs

			// Check all outputs. If any are fully defined, all nodes connected to that output can instead be connected to a const-node with the result.
			// If this nodes ends up without any other nodes connected to it, it will be culled by other optimization steps.
			for (size_t port : utils::Range(successor.node->getNumOutputPorts())) {
				auto conType = successor.node->getOutputConnectionType(port);

				bool allDefined = true;
				for (auto i : utils::Range(conType.width))
					if (!state.get(sim::DefaultConfig::DEFINED, outputOffsets[port]+i)) {
						allDefined = false;
						break;
					}

				if (allDefined) {
					//std::cout << "	Found all const output" << std::endl;

					auto* constant = createNode<Node_Constant>(state.extract(outputOffsets[port], conType.width), conType.interpretation);
					constant->moveToGroup(successor.node->getGroup());
					subnet.add(constant);
					NodePort newConstOutputPort{.node = constant, .port = 0};

					while (!successor.node->getDirectlyDriven(port).empty()) {
						NodePort input = successor.node->getDirectlyDriven(port).back();
						input.node->connectInput(input.port, newConstOutputPort);
					}

					// Add the new const-node output to the openList to continue const-propagation from here.
					openList.push_back(newConstOutputPort);
				}
			}
		}
	}
}

void Circuit::removeFalseLoops()
{
//	for (size_t i = 0; i < m_nodes.size(); i++)
//	{
//		auto* signalNode = dynamic_cast<const Node_Signal*>(m_nodes[i].get());
//
//		if (signalNode && !signalNode->getNonSignalDriver(0).node)
//		{
//			if (i+1 != m_nodes.size())
//			  m_nodes[i] = std::move(m_nodes.back());
//			m_nodes.pop_back();
//			i--;
//		}
//	}
}

/// @details It seems many parts of the vhdl export still require signal nodes so this step adds back in missing ones
void Circuit::ensureSignalNodePlacement()
{
	std::map<NodePort, Node_Signal*> addedSignalsNodes;

	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();
		if (dynamic_cast<Node_Signal*>(node) != nullptr) continue;
		for (auto i : utils::Range(node->getNumInputPorts())) {
			auto driver = node->getDriver(i);
			if (driver.node == nullptr) continue;
			if (node->getDriverConnType(i).interpretation == ConnectionType::DEPENDENCY) continue;
			if (dynamic_cast<Node_Signal*>(driver.node) != nullptr) continue;
			if (driver.node->getGroup() != nullptr && driver.node->getGroup()->getGroupType() == NodeGroup::GroupType::SFU) continue;

			auto it = addedSignalsNodes.find(driver);
			if (it != addedSignalsNodes.end()) {
				node->connectInput(i, {.node=it->second, .port=0ull});
			} else {
				auto *sigNode = createNode<Node_Signal>();
				sigNode->moveToGroup(driver.node->getGroup());

				sigNode->connectInput(driver);
				node->connectInput(i, {.node=sigNode, .port=0ull});
				addedSignalsNodes[driver] = sigNode;
			}
		}
	}
}

/**
 * @brief Split/duplicate signal nodes feeding into lower and higher areas of the hierarchy.
 *
 * When generating a signal in any given area, it is possible to feed that signal to the parent (an output of said area)
 * and simultaneously to a child area (an input to the child area).
 * By default, the VHDL exporter declares this signal as an output signal (part of the port map) and feeds that signal to the 
 * child area as well. While this is ok with ghdl, intel quartus does not accept this, so we have to duplicate the signal for quartus in
 * order to ensure that a local, non-port-map signal gets bound to the child area.
 */
void Circuit::duplicateSignalsFeedingLowerAndHigherAreas()
{
	std::vector<NodePort> higherDriven;
	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();

		for (auto i : utils::Range(node->getNumOutputPorts())) {
			higherDriven.clear();
			//bool consumedInternally = false;
			bool consumedHigher = false;
			bool consumedLower = false;

			for (auto driven : node->getDirectlyDriven(0)) {
				if (driven.node->getGroup() == node->getGroup()) {
					//consumedInternally = true;
				} else if (driven.node->getGroup() != nullptr && driven.node->getGroup()->isChildOf(node->getGroup())) {
					consumedLower = true;
				} else {
					consumedHigher = true;
					higherDriven.push_back(driven);
				}
			}

			if (consumedHigher && consumedLower) {
				auto *sigNode = createNode<Node_Signal>();
				sigNode->moveToGroup(node->getGroup());
				sigNode->connectInput({.node = node, .port = i});

				for (auto driven : higherDriven)
					driven.node->rewireInput(driven.port, {.node = sigNode, .port = 0});
			}
		}
	}
}


void Circuit::optimizeSubnet(Subnet &subnet)
{
	//defaultValueResolution(*this, subnet);
	//cullUnusedNodes(subnet); // Dirty way of getting rid of default nodes
	
	propagateConstants(subnet);
	mergeRewires(subnet);
	optimizeRewireNodes(subnet);
	mergeMuxes(subnet);
	removeIrrelevantComparisons(subnet);
	removeIrrelevantMuxes(subnet);
	cullMuxConditionNegations(subnet);
	removeNoOps(subnet);
	foldRegisterMuxEnableLoops(subnet);
	removeConstSelectMuxes(subnet);
	propagateConstants(subnet); // do again after muxes are removed
	cullUnusedNodes(subnet);
}


void Circuit::postprocess(const PostProcessor &postProcessor)
{
	dbg::changeState(dbg::State::POSTPROCESS);
	postProcessor.run(*this);

	detectUnguardedCDCCrossings(*this, ConstSubnet::all(*this), [](const BaseNode *affectedNode, size_t) {
		dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_POSTPROCESSING 
				<< "Unintentional clock domain crossing detected on inputs to node " << affectedNode
		);
		HCL_DESIGNCHECK_HINT(false, "Unintentional clock domain crossing detected!");
	});

}

void DefaultPostprocessing::generalOptimization(Circuit &circuit) const
{
	Subnet subnet = Subnet::all(circuit);
	circuit.disconnectZeroBitSignalNodes();
	circuit.disconnectZeroBitOutputPins();
	defaultValueResolution(circuit, subnet);
	circuit.cullUnusedNodes(subnet); // Dirty way of getting rid of default nodes
	
	subnet = Subnet::all(circuit);
	resolveRetimingHints(circuit, subnet);

	circuit.propagateConstants(subnet);
	circuit.cullOrphanedSignalNodes();
	circuit.cullUnnamedSignalNodes();
	circuit.cullSequentiallyDuplicatedSignalNodes();
	subnet = Subnet::all(circuit);
	circuit.mergeRewires(subnet);
	circuit.optimizeRewireNodes(subnet);
	circuit.mergeMuxes(subnet);
	circuit.removeIrrelevantComparisons(subnet);
	circuit.removeIrrelevantMuxes(subnet);	
	circuit.cullMuxConditionNegations(subnet);
	circuit.removeNoOps(subnet);
	circuit.foldRegisterMuxEnableLoops(subnet);
	circuit.removeConstSelectMuxes(subnet);
	circuit.propagateConstants(subnet); // do again after muxes are removed
	circuit.cullUnusedNodes(subnet);

	attributeFusion(circuit);
}

void DefaultPostprocessing::memoryDetection(Circuit &circuit) const
{
	findMemoryGroups(circuit);
	circuit.cullUnnamedSignalNodes();

	Subnet subnet = Subnet::all(circuit);
	circuit.cullUnusedNodes(subnet); // do again after memory group extraction with potential register retiming
}

void DefaultPostprocessing::exportPreparation(Circuit &circuit) const
{
	circuit.ensureSignalNodePlacement();
	circuit.duplicateSignalsFeedingLowerAndHigherAreas();
	circuit.inferSignalNames();
}



void DefaultPostprocessing::run(Circuit &circuit) const
{
	dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Running default postprocessing.");

	generalOptimization(circuit);
	memoryDetection(circuit);

	if (m_techMapping) {
		m_techMapping->apply(circuit, circuit.getRootNodeGroup());
	} else {
		TechnologyMapping mapping;
		mapping.apply(circuit, circuit.getRootNodeGroup());
	}
	generalOptimization(circuit); // Because we ran frontend code for tech mapping

	exportPreparation(circuit);
}


Node_Signal *Circuit::appendSignal(NodePort &nodePort)
{
	Node_Signal *sig = createNode<Node_Signal>();
	sig->recordStackTrace();
	if (nodePort.node != nullptr) {
		sig->connectInput(nodePort);
		sig->moveToGroup(nodePort.node->getGroup());
	}
	nodePort = {.node=sig, .port=0ull};
	return sig;
}

Node_Signal *Circuit::appendSignal(RefCtdNodePort &nodePort)
{
	Node_Signal *sig = createNode<Node_Signal>();
	sig->recordStackTrace();
	if (nodePort.node != nullptr) {
		sig->connectInput(nodePort);
		sig->moveToGroup(nodePort.node->getGroup());
	}
	nodePort = {.node=sig, .port=0ull};
	return sig;
}

Node_Attributes *Circuit::getCreateAttribNode(NodePort &nodePort)
{
	for (auto nh : nodePort.node->exploreOutput(nodePort.port)) {
		if (auto *attribNode = dynamic_cast<Node_Attributes*>(nh.node())) return attribNode;
		if (!nh.isSignal()) nh.backtrack();
	}

	
	auto *attribNode = createNode<Node_Attributes>();
	attribNode->connectInput(nodePort); /// @todo: place after signal node?
	attribNode->recordStackTrace();
	attribNode->moveToGroup(nodePort.node->getGroup());
	return attribNode;
}

}
