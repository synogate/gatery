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
#include "RevisitCheck.h"

#include "../debug/DebugInterface.h"
#include "../export/DotExport.h"

#include "NodeGroup.h"
#include "SignalGroup.h"
#include "Node.h"
#include "Clock.h"
#include "CNF.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Multiplexer.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Pin.h"
#include "coreNodes/Node_Rewire.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Compare.h"
#include "coreNodes/Node_Signal2Clk.h"
#include "coreNodes/Node_Signal2Rst.h"
#include "coreNodes/Node_MultiDriver.h"

#include "supportNodes/Node_Attributes.h"
#include "supportNodes/Node_External.h"
#include "supportNodes/Node_MemPort.h"
#include "supportNodes/Node_ExportOverride.h"

#include "postprocessing/MemoryDetector.h"
#include "postprocessing/DefaultValueResolution.h"
#include "postprocessing/AttributeFusion.h"
#include "postprocessing/TechnologyMapping.h"
#include "postprocessing/Retiming.h"
#include "postprocessing/CDCDetection.h"


#include "../simulation/BitVectorState.h"
#include "../simulation/ReferenceSimulator.h"
#include "../simulation/SimulationVisualization.h"
#include "../simulation/simProc/SimulationProcess.h"
#include "../utils/Range.h"
#include "GraphTools.h"


#include "../export/DotExport.h"

#include "Subnet.h"


#include <set>
#include <vector>
#include <map>

#include <iostream>

template class std::unique_ptr<gtry::hlim::BaseNode>;
template class std::unique_ptr<gtry::hlim::Clock>;
template class std::unique_ptr<gtry::hlim::SignalGroup>;

namespace gtry::hlim {

Circuit::Circuit(std::string_view topName)
{
#ifdef _DEBUG
	readDebugNodeIds();
#endif

	m_root.reset(new NodeGroup(*this, NodeGroupType::ENTITY, topName, nullptr));
}

Circuit::~Circuit()
{
}

/**
 * @brief Copies a subnet of all nodes that are needed to drive the specified outputs up to the specified inputs
 * @note Do note that subnetInputs specified input ports, not output ports!
 * @param source The circuit to copy from
 * @param subnetInputs Input ports where to stop copying
 * @param subnetOutputs Output ports from where to start copying.
 * @param mapSrc2Dst Map from all copied nodes in the source circuit to the corresponding node in the destination circuit.
 */
void Circuit::copySubnet(const utils::StableSet<NodePort> &subnetInputs,
						 const utils::StableSet<NodePort> &subnetOutputs,
						 utils::StableMap<BaseNode*, BaseNode*> &mapSrc2Dst,
						 bool copyClocks)
{
	mapSrc2Dst.clear();

	utils::UnstableSet<BaseNode*> closedList;
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
	utils::StableMap<Clock*, Clock*> mapSrc2Dst_clocks;
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
			if (oldClock != nullptr) {
				if (copyClocks)
					newNode->attachClock(lazyCreateClockNetwork(oldClock), i);
				else
					newNode->attachClock(oldClock, i);
			}
		}
	}
}


BaseNode *Circuit::createUnconnectedClone(BaseNode *srcNode, bool noId)
{
	m_nodes.push_back(srcNode->cloneUnconnected());
	m_nodes.back()->moveToGroup(m_root.get());
	if (!noId)
		m_nodes.back()->setId(m_nextNodeId++, {});
	return m_nodes.back().get();
}


Clock *Circuit::createUnconnectedClock(Clock *clock, Clock *newParent)
{
	m_clocks.push_back(clock->cloneUnconnected(newParent));
	m_clocks.back()->setId(m_nextClockId++, {});
	return m_clocks.back().get();
}

/**
 * @brief Infers signal names for all unnamed signals
 *
 */
void Circuit::inferSignalNames()
{
	utils::StableSet<Node_Signal*> unnamedSignals;
	for (size_t i = 0; i < m_nodes.size(); i++)
		if (auto *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get()))
			if (signal->getName().empty()) {
				auto driver = signal->getDriver(0);
				if (driver.node != nullptr) {
					if (signal->inputIsComingThroughParentNodeGroup(0)) {
						signal->setInferredName("unnamed");
					} else {
						auto name = driver.node->attemptInferOutputName(driver.port);
						if (!name.empty()) {
							if (name.size() > 200)
								signal->setInferredName("unnamed");
							else
								signal->setInferredName(std::move(name));
						} else
							unnamedSignals.insert(signal);
					}
				} else 
					signal->setInferredName("undefined");
			}


	while (!unnamedSignals.empty()) {

		Node_Signal *s = *unnamedSignals.begin();
		std::vector<Node_Signal*> signalsToName;
		utils::UnstableSet<BaseNode*> loopDetection;

		signalsToName.push_back(s);
		loopDetection.insert(s);

		for (auto nh : s->exploreInput(0).skipDependencies()) {
			if (loopDetection.contains(nh.node())) {
				nh.backtrack();
				continue;
			}
			if (nh.isSignal()) {
				if (!nh.node()->getName().empty()) {
					nh.backtrack();
				} else {
					signalsToName.push_back((Node_Signal*)nh.node());
				}
			}
			loopDetection.insert(nh.node());
		}

		for (auto it = signalsToName.rbegin(); it != signalsToName.rend(); ++it) {
			if ((*it)->getName().empty()) {
				auto driver = (*it)->getDriver(0);
				if (driver.node != nullptr) {
					if ((*it)->inputIsComingThroughParentNodeGroup(0))
						(*it)->setInferredName("unnamed");
					else {
						auto name = driver.node->attemptInferOutputName(driver.port);
						if (!name.empty()) {
							if (name.size() > 200)
								(*it)->setInferredName("unnamed");
							else
								(*it)->setInferredName(std::move(name));
						}
					}
				} else
					(*it)->setInferredName("undefined");
			}
			unnamedSignals.erase(*it);
		}
	}
}

void Circuit::disconnectZeroBitConnections()
{
	auto buildZeroBitConst = [this](NodeGroup *group) {
		sim::DefaultBitVectorState undef;
		undef.resize(0);
		auto* constant = createNode<Node_Constant>(std::move(undef), ConnectionType{ .type = ConnectionType::BITVEC, .width = 0 });
		constant->moveToGroup(group);

		return constant;
	};

	for (size_t i = 0; i < m_nodes.size(); i++) {
		for (size_t port = 0; port < m_nodes[i]->getNumInputPorts(); port++) {
			auto driver = m_nodes[i]->getDriver(port);
			if (driver.node != nullptr) {
				if (getOutputConnectionType(driver).type != ConnectionType::DEPENDENCY && getOutputConnectionType(driver).width == 0) {
					m_nodes[i]->rewireInput(port, { .node = buildZeroBitConst(m_nodes[i]->getGroup()), .port = 0 });
				}
			}
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
			dbg::log(dbg::LogMessage(pin->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing output pin " << pin << " because it's output is of width zero");
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
 * @brief Searches for any signal nodes that are unconnected or form signal loops and insert const undefined nodes.
 * 
 */
void Circuit::insertConstUndefinedNodes()
{
	auto buildConstUndefFor = [this](Node_Signal *signal) {
		sim::DefaultBitVectorState undef;
		undef.resize(signal->getOutputConnectionType(0).width);
		auto* constant = createNode<Node_Constant>(std::move(undef), signal->getOutputConnectionType(0).type);
		constant->moveToGroup(signal->getGroup());

		return constant;
	};

	for (size_t i = 0; i < m_nodes.size(); i++) {
		Node_Signal *signal = dynamic_cast<Node_Signal*>(m_nodes[i].get());
		if (signal == nullptr)
			continue;
		if (signal->getDriver(0).node == nullptr) {
			auto *constant = buildConstUndefFor(signal);
			signal->rewireInput(0, {.node = constant, .port = 0ull});
			dbg::log(dbg::LogMessage(signal->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Prepended undriven signal node " << signal << " with const undefined node " << constant);
		} else {
			RevisitCheck alreadyVisited(*this);

			auto driver = signal->getDriver(0);
			while (dynamic_cast<Node_Signal*>(driver.node)) {
				if (alreadyVisited.contains(driver.node)) {
					// loop
					auto *constant = buildConstUndefFor(signal);
					signal->rewireInput(0, {.node = constant, .port = 0ull});

					dbg::log(dbg::LogMessage(signal->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Splitting signal loop on node " << signal << " with const undefined node " << constant);
					break;
				}
				alreadyVisited.insert(driver.node);
				driver = driver.node->getDriver(0);
			}
		}
	}
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

		// Don't cull loopy signals
		if (signal->getDriver(0).node == signal) continue;

		// We want to keep one signal node between non-signal nodes, so only cull if the input or all outputs are signals.
		bool inputIsSignalOrUnconnected = (signal->getDriver(0).node == nullptr) || (dynamic_cast<Node_Signal*>(signal->getDriver(0).node) != nullptr);

		// Don't cull if inputs is from another, higher group
		if (!signal->inputIsComingThroughParentNodeGroup(0)) {

			bool allOutputsAreSignals = true;
			for (const auto &c : signal->getDirectlyDriven(0)) {
				allOutputsAreSignals &= (dynamic_cast<Node_Signal*>(c.node) != nullptr);
			}

			if (inputIsSignalOrUnconnected || allOutputsAreSignals) {
				if (signal->getDriver(0).node != signal)
					signal->bypassOutputToInput(0, 0);
				signal->disconnectInput();

				dbg::log(dbg::LogMessage(signal->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Culling unnamed signal node " << signal);

				if (i+1 != m_nodes.size())
					m_nodes[i] = std::move(m_nodes.back());
				m_nodes.pop_back();
				i--;
			}
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
				dbg::log(dbg::LogMessage(signal->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Culling sequentially duplicated signal node " << signal);

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
		usedNodes.addDrivenNamedSignals(*this);

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





void Circuit::mergeMuxes(Subnet &subnet)
{
	bool done;
	do {
		done = true;

		for (size_t i = 0; i < m_nodes.size(); i++) {
			if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
				if (muxNode->getNumInputPorts() != 3) continue;

				//std::cout << "Found 2-input mux" << std::endl;

				Conjunction condition;
				condition.parseInput({.node = muxNode, .port = 0});

				for (size_t muxInput : utils::Range(2)) {

					auto input0 = muxNode->getNonSignalDriver(muxInput?2:1);
					auto input1 = muxNode->getNonSignalDriver(muxInput?1:2);

					if (input1.node == nullptr)
						continue;

					if (Node_Multiplexer *prevMuxNode = dynamic_cast<Node_Multiplexer*>(input0.node)) {
						if (prevMuxNode->getNumInputPorts() != 3) continue;
						if (prevMuxNode == muxNode) continue; // sad thing
						if (!subnet.contains(prevMuxNode)) continue; // todo: Optimize

						//std::cout << "Found 2 chained muxes" << std::endl;

						Conjunction prevCondition;
						prevCondition.parseInput({.node = prevMuxNode, .port = 0});

						bool conditionsMatch = false;
						bool prevConditionNegated;

						if (prevCondition.isEqualTo(condition)) {
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
							if (prevMuxNode->getNonSignalDriver(prevConditionNegated?2:1) == muxNode->getNonSignalDriver(muxInput?2:1))
								dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_POSTPROCESSING << "Not merging muxes " << prevMuxNode << " and " << muxNode << " because the former is driving itself.");
							else {
								//std::cout << "Conditions match!" << std::endl;
								dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Merging muxes " << prevMuxNode << " and " << muxNode << " because they form an if then else pair");

								auto bypass = prevMuxNode->getDriver(prevConditionNegated?2:1);

								// Connect second mux directly to bypass
								muxNode->connectInput(muxInput, bypass);

								done = false;
							}
						}
					}
				}
			}
		}
	} while (!done);
}


void Circuit::breakMutuallyExclusiveMuxChains(Subnet& subnet)
{
#if 0
	for (size_t i = 0; i < m_nodes.size(); i++) {
		if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(m_nodes[i].get())) {
			if (muxNode->getNumInputPorts() != 3) continue;

			//std::cout << "Found 2-input mux" << std::endl;

			Conjunction condition;
			condition.parseInput({.node = muxNode, .port = 0});

			// So far, CNF can't properly handle negations, so we are restricted to only checking a chain along the "false" path
			for (size_t muxInput : { 1 }) {

				// Scan the "condition = true" input and check for any mux whose enable condition can not be true as well
				for (auto nh : muxNode->exploreInput(muxInput)) {

				}

				auto input0 = muxNode->getNonSignalDriver(muxInput?2:1);
				auto input1 = muxNode->getNonSignalDriver(muxInput?1:2);

				if (input1.node == nullptr)
					continue;

				if (Node_Multiplexer *prevMuxNode = dynamic_cast<Node_Multiplexer*>(input0.node)) {
					if (prevMuxNode->getNumInputPorts() != 3) continue;
					if (prevMuxNode == muxNode) continue; // sad thing
					if (!subnet.contains(prevMuxNode)) continue; // todo: Optimize

					Conjunction prevCondition;
					prevCondition.parseInput({.node = prevMuxNode, .port = 0});

					bool conditionsMatch = false;
					bool prevConditionNegated;

					if (prevCondition.cannotBothBeTrue(condition)) {



/*

						if (prevMuxNode->getNonSignalDriver(prevConditionNegated?2:1) == muxNode->getNonSignalDriver(muxInput?2:1))
							dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_POSTPROCESSING << "Not merging muxes " << prevMuxNode << " and " << muxNode << " because the former is driving itself.");
						else {
							//std::cout << "Conditions match!" << std::endl;
							dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Merging muxes " << prevMuxNode << " and " << muxNode << " because they form an if then else pair");

							auto bypass = prevMuxNode->getDriver(prevConditionNegated?2:1);

							// Connect second mux directly to bypass
							muxNode->connectInput(muxInput, bypass);

							//done = false;
						}
						*/
					}
				}
			}
		}
	}
#endif
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

								if (prevRewireNode->getNonSignalDriver(prevInputIdx) == rewireNode->getNonSignalDriver(inputIdx))
									dbg::log(dbg::LogMessage(rewireNode->getGroup()) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_POSTPROCESSING << "Not merging rewires " << prevRewireNode << " and " << rewireNode << " because the former is driving itself.");
								else {
									dbg::log(dbg::LogMessage(rewireNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Merging rewires " << prevRewireNode << " and " << rewireNode << " by directly fetching from input " << prevInputIdx << " at bit offset " << prevInputOffset);

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

				Conjunction condition;
				condition.parseInput({.node = muxNode, .port = 0});

				for (size_t muxInputPort : utils::Range(1,3)) {

					for (auto muxOutput : muxNode->getDirectlyDriven(0)) {
						std::vector<NodePort> openList = { muxOutput };
						utils::UnstableSet<NodePort> closedList;

						bool allSubnetOutputsMuxed = true;

						while (!openList.empty()) {
							NodePort input = openList.back();
							openList.pop_back();
							if (closedList.contains(input)) continue;
							closedList.insert(input);

							if (input.node->hasSideEffects() || input.node->hasRef() || !input.node->isCombinatorial(input.port)) {
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
								if (subnetOutputMuxNode->getNumInputPorts() == 3) {
									Conjunction subnetOutputMuxNodeCondition;
									subnetOutputMuxNodeCondition.parseInput({.node = subnetOutputMuxNode, .port = 0});

									if (input.port == muxInputPort && condition.isEqualTo(subnetOutputMuxNodeCondition))
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
							if (muxNode->getNonSignalDriver(muxInputPort) == muxOutput.node->getNonSignalDriver(muxOutput.port))
								dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_POSTPROCESSING << "Not removing mux " << muxNode << " because it is driving itself");
							else {
								dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because only input " << muxInputPort << " is relevant further on");
								//std::cout << "Rewiring past mux" << std::endl;
								muxOutput.node->connectInput(muxOutput.port, muxNode->getDriver(muxInputPort));
								done = false;
							}
						} else {
							//std::cout << "Not rewiring past mux" << std::endl;
						}
					}
				}
			}
		}
	} while (!done);
}


std::optional<std::pair<Node_Multiplexer*, Node_Constant*>> followedByCompatibleMux(Node_Multiplexer *muxNode, NodePort comparisonSignal, RevisitCheck &cycleCheck)
{
	for (auto nh : muxNode->exploreOutput(0)) {
		if (cycleCheck.contains(nh.node())) {
			nh.backtrack();
			continue;
		}
		cycleCheck.insert(nh.node());

		if (auto *nextMuxNode = dynamic_cast<Node_Multiplexer*>(nh.node())) {
			if (nextMuxNode->getNumInputPorts() == 3) {
				auto nextComparison = isComparisonWithConstant(nextMuxNode->getNonSignalDriver(0));
				if (nextComparison)
					if (nextComparison->second == comparisonSignal)
						if (sim::allDefined(nextComparison->first->getValue()))
							return { std::make_pair(nextMuxNode, nextComparison->first) };
			}

			nh.backtrack();
			continue;
		}
		
		if (!nh.isSignal()) {
			nh.backtrack();
			continue;
		}
	}
	return {};
}

/**
 * @brief Merge a chain of binary muxes, selected by comparisons, into a single mux.
 * @details The mux node is capable of multiplexing between more than two inputs, based on a bitvector selector index.
 * Yet quite often, the frontend constructs large chains of binary multiplexers, each being selected by a comparison between
 * an index and a constant. This code detects these chains and turns them back into a single multiplexer.
 */
void Circuit::mergeBinaryMuxChain(Subnet& subnet)
{
	std::vector<BaseNode*> newNodes;

	utils::UnstableSet<Node_Multiplexer *> alreadyHandled;
	for (auto n : subnet) {
		if (Node_Multiplexer *muxNode = dynamic_cast<Node_Multiplexer*>(n)) {
			if (alreadyHandled.contains(muxNode)) continue;

			if (muxNode->getNumInputPorts() != 3) continue;
			auto comparison = isComparisonWithConstant(muxNode->getNonSignalDriver(0));
			if (!comparison) continue;

			if (!sim::allDefined(comparison->first->getValue())) continue;

			RevisitCheck cycleCheck(*this);
			cycleCheck.insert(muxNode);

			std::vector<std::pair<Node_Multiplexer*, Node_Constant*>> chain;
			chain.emplace_back(muxNode, comparison->first);


			// scan backwards
			{
				Node_Multiplexer *backNode = muxNode;
				while (true) {
					backNode = dynamic_cast<Node_Multiplexer*>(backNode->getNonSignalDriver(1).node);
					if (!backNode) break;

					if (cycleCheck.contains(backNode)) break;
					cycleCheck.insert(backNode);

					if (backNode->getNumInputPorts() != 3) break;
					auto backComparison = isComparisonWithConstant(backNode->getNonSignalDriver(0));
					if (!backComparison) break;
					if (!sim::allDefined(backComparison->first->getValue())) break;


					// The value compared with on every mux must be the same
					if (backComparison->second != comparison->second) break;

					chain.emplace_back(backNode, backComparison->first);
				}
			}

			// We inserted stuff while traveling backwards, so reverse to bring into proper order:
			std::reverse(chain.begin(), chain.end());

			// scan forwards
			{
				Node_Multiplexer *forwardNode = muxNode;
				while (true) {

					auto next = followedByCompatibleMux(forwardNode, comparison->second, cycleCheck);
					if (!next) break;

					forwardNode = next->first;
						
					chain.emplace_back(next->first, next->second);
				}
			}

			for (auto &e : chain)
				alreadyHandled.insert(e.first);

			size_t width = getOutputWidth(comparison->second);

			// At least 3 muxes and, if joined, at least one quarter of the inputs populated
			if (chain.size() >= 3 && chain.size()*4 >= (1ull << width)) {
				std::vector<NodePort> inputs;
				// Default to the "false" path through the mux chain
				inputs.resize(1ull << width, chain[0].first->getDriver(1));
				// Progress forward through the mux chain, potentially overriding if multiple muxes check on the same value
				for (const auto &c : chain) {
					HCL_ASSERT(sim::allDefined(c.second->getValue())); // Was checked and ensured before

					size_t idx = c.second->getValue().extractNonStraddling(sim::DefaultConfig::VALUE, 0, c.second->getValue().size());
					inputs[idx] = c.first->getDriver(2);
				}

				auto *newMux = createNode<Node_Multiplexer>(inputs.size());
				newNodes.push_back(newMux);
				newMux->recordStackTrace();
				newMux->moveToGroup(chain.back().first->getGroup());
				newMux->connectSelector(comparison->second);
				for (size_t i = 0; i < inputs.size(); i++) 
					newMux->connectInput(i, inputs[i]);

				auto allToRewire = chain.back().first->getDirectlyDriven(0);
				for (auto np : allToRewire)
					np.node->rewireInput(np.port, {.node = newMux, .port = 0ull} );

				dbg::log(dbg::LogMessage(newMux->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING 
						<< "Replacing chain of mux nodes starting at node " << chain.front().first 
						<< " and ending at " << chain.back().first 
						<< " with one big switching mux." << newMux);

			}
		}
	}

	for (auto n : newNodes)
		subnet.add(n);
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
			if (getOutputConnectionType(leftDriver).type != ConnectionType::BOOL ||
				getOutputConnectionType(rightDriver).type != ConnectionType::BOOL)
				continue;

			Node_Constant *constInputs[2];
			constInputs[0] = dynamic_cast<Node_Constant*>(leftDriver.node);
			constInputs[1] = dynamic_cast<Node_Constant*>(rightDriver.node);

			for (auto i : utils::Range(2)) {
				if (constInputs[i] == nullptr) continue;
				if (constInputs[i]->getValue().get(sim::DefaultConfig::DEFINED, 0) == false) {
					dbg::log(dbg::LogMessage(compNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
									<< " is comparing to an undefined signal coming from " << constInputs[i] << ". Hardwireing output to undefined source " << compNode->getDriver(i).node);
					// replace with undefined
					compNode->bypassOutputToInput(0, i);
					break;
				}
				bool invert = constInputs[i]->getValue().get(sim::DefaultConfig::VALUE, 0) ^ (compNode->getOp() == Node_Compare::EQ);
				if (invert) {
					// replace with inverter
					auto *notNode = createNode<Node_Logic>(Node_Logic::NOT);

					dbg::log(dbg::LogMessage(compNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
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
					dbg::log(dbg::LogMessage(compNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Compare node " << compNode 
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
						if (logicNode->getNonSignalDriver(0).node == logicNode)
							dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_POSTPROCESSING << "Not removing/bypassing negation " << logicNode << " because it is driving itself");
						else {
							muxNode->connectSelector(logicNode->getDriver(0));

							auto input0 = muxNode->getDriver(1);
							auto input1 = muxNode->getDriver(2);

							muxNode->connectInput(0, input1);
							muxNode->connectInput(1, input0);

							dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing/bypassing negation " << logicNode << " to mux " << muxNode << " selector by swapping mux inputs.");

							done = false; // check same mux again to unravel chain of NOTs
						}
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

		if (m_nodes[i]->bypassIfNoOp()) {
			dbg::log(dbg::LogMessage(m_nodes[i].get()->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing " << m_nodes[i].get() << " because it is a no-op.");
			removeNode = true;
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

							dbg::log(dbg::LogMessage(regNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Replacing mux loop through " << muxNode << " on register " << regNode << " by and-ing the condition to register enable through " << andNode);
						} else {
							regNode->connectInput(Node_Register::Input::ENABLE, muxCondition);

							dbg::log(dbg::LogMessage(regNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Replacing mux loop through " << muxNode << " on register " << regNode << " by binding the condition to register enable");
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

							dbg::log(dbg::LogMessage(regNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Replacing mux loop through " << muxNode << " on register " << regNode << " by and-ing the condition to register enable through " << andNode);
						} else {
							regNode->connectInput(Node_Register::Input::ENABLE, {.node=notNode, .port=0ull});

							dbg::log(dbg::LogMessage(regNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Replacing mux loop through " << muxNode << " on register " << regNode << " by binding the condition to register enable");
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
					dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because its selector is constant, empty, and defaults to zero.");
					muxNode->bypassOutputToInput(0, 1);
				}
				else
				{
					HCL_ASSERT(constNode->getValue().size() < 64);
					std::uint64_t selDefined = constNode->getValue().extractNonStraddling(sim::DefaultConfig::DEFINED, 0, constNode->getValue().size());
					std::uint64_t selValue = constNode->getValue().extractNonStraddling(sim::DefaultConfig::VALUE, 0, constNode->getValue().size());
					if ((selDefined ^ (~0ull >> (64 - constNode->getValue().size()))) == 0) {
						dbg::log(dbg::LogMessage(muxNode->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing mux " << muxNode << " because its selector is constant and defined.");
						muxNode->bypassOutputToInput(0, 1+selValue);
					}
				}
			}
		}
	}
}

void Circuit::propagateConstants(Subnet &subnet)
{

	// Find all nodes that may still be overriden because handles exist to them
	// Since registers can be overriden from signal nodes, this property needs to be "backpropagated" to driving registers.

	Subnet overridableNodes;
	for (const auto &n : m_nodes) {
		if (n->hasRef()) {
			// This one must not be folded
			overridableNodes.add(n.get());

			// All registers driving this node also must not be folded.
			for (auto i : utils::Range(n->getNumInputPorts())) {
				auto driver = n->getNonSignalDriver(i);
				if (dynamic_cast<Node_Register*>(driver.node))
					overridableNodes.add(driver.node);
			}
		}
		// Also add export override nodes to this list, as they can behave differently on export and we should not constant fold through them.
		if (dynamic_cast<Node_ExportOverride*>(n.get()))
			overridableNodes.add(n.get());
	}


	//std::cout << "propagateConstants()" << std::endl;
	sim::SimulatorCallbacks ignoreCallbacks;

	std::vector<NodePort> openList;
	// std::set<NodePort> closedList;

	// Start walking the graph from nodes with constant output
	std::vector<BaseNode*> newNodes;
	for (auto n : subnet)
		for (size_t port = 0; port < n->getNumOutputPorts(); port++)
			if (n->outputIsConstant(port)) {
				if (dynamic_cast<Node_Constant*>(n))
					openList.push_back({.node = n, .port = port});
				else {
					auto value = evaluateStatically(*this, {.node = n, .port = port});
					auto *newConstant = createNode<Node_Constant>(value, n->getOutputConnectionType(port));
					newNodes.push_back(newConstant);
					newConstant->moveToGroup(n->getGroup());
					auto driven = n->getDirectlyDriven(port);
					for (auto &d : driven)
						d.node->rewireInput(d.port, {.node = newConstant, .port = 0});

					openList.push_back({.node = newConstant, .port = port});
				}
			}

	for (auto &n : newNodes)
		subnet.add(n);

	while (!openList.empty()) {
		NodePort constPort = openList.back();
		openList.pop_back();

		// If this node is overridable, skip it.
		if (overridableNodes.contains(constPort.node))
			continue;
		/*
		if (closedList.contains(constPort)) continue;
		closedList.insert(constPort);
		*/

		// The output constPort is constant, loop over all nodes driven by this and look for nodes that can be computed
		auto& nodeList = constPort.node->getDirectlyDriven(constPort.port);
		for(size_t i = 0; i < nodeList.size(); ++i)
		{
			NodePort successor = nodeList[i];

			// If this node is overridable, skip it.
			if (overridableNodes.contains(successor.node))
				continue;

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
						sim::ReferenceSimulator simulator(false);
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

				// Only consider on combinatory outputs
				if (!successor.node->isCombinatorial(port)) continue;

				auto conType = successor.node->getOutputConnectionType(port);

				bool allDefined = true;
				for (auto i : utils::Range(conType.width))
					if (!state.get(sim::DefaultConfig::DEFINED, outputOffsets[port]+i)) {
						allDefined = false;
						break;
					}

				if (allDefined) {
					//std::cout << "	Found all const output" << std::endl;

					auto* constant = createNode<Node_Constant>(state.extract(outputOffsets[port], conType.width), conType.type);
					constant->moveToGroup(successor.node->getGroup());
					subnet.add(constant);
					NodePort newConstOutputPort{.node = constant, .port = 0};

					dbg::log(dbg::LogMessage(successor.node->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Replacing " << successor.node << " port " << port << " with folded constant " << constant);

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

void Circuit::ensureEntityPortSignalNodes()
{
	utils::UnstableMap<std::pair<NodePort, NodeGroup*>, Node_Signal*> addedSignalsNodes;

	auto insertSignalNode = [&addedSignalsNodes, this](NodePort driven, NodeGroup *group) -> Node_Signal* {
		auto driver = driven.node->getDriver(driven.port);

		// Check if such a signal already exists (to avoid too many duplicates)
		Node_Signal *signal;
		auto it = addedSignalsNodes.find(std::make_pair(driver, group));
		if (it != addedSignalsNodes.end())
			signal = it->second;
		else {
			signal = createNode<Node_Signal>();
			signal->recordStackTrace();
			signal->moveToGroup(group);
			signal->connectInput(driver);
			addedSignalsNodes[std::make_pair(driver, group)] = signal;
		}
		driven.node->rewireInput(driven.port, {.node = signal, .port = 0ull});
		return signal;
	};

	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();

		if (node->getGroup()->getGroupType() == NodeGroupType::SFU)
			continue;

		auto *extSrcNode = dynamic_cast<Node_External*>(node);
		auto *multiDriverSrcNode = dynamic_cast<Node_MultiDriver*>(node);

		// Check all outputs that are not dependencies
		for (auto outputIdx : utils::Range(node->getNumOutputPorts())) {
			if (node->getOutputConnectionType(outputIdx).type == ConnectionType::DEPENDENCY) continue;

			// Check if this is a bidirectional connection, in which case no signal nodes but multi-driver nodes must be placed (by a different funtion).
			if (extSrcNode && extSrcNode->getOutputPorts()[outputIdx].bidirPartner != 0) continue;

			// Check all driven by this port if they are in a different node group
			auto allDriven = node->getDirectlyDriven(0);
			for (auto &driven : allDriven) {
				if (driven.node->getGroup() == node->getGroup())
					continue;

				// Check if this is a bidirectional connection, in which case no signal nodes but multi-driver nodes must be placed (by a different funtion).
				if (auto *extDstNode = dynamic_cast<Node_External*>(driven.node))
					if (extDstNode->getInputPorts()[driven.port].bidirPartner != 0) continue;
				auto *multiDriverDstNode = dynamic_cast<Node_MultiDriver*>(driven.node);
				if (multiDriverSrcNode && multiDriverDstNode) continue;

				// if this is a child node group
				if (driven.node->getGroup()->isChildOf(node->getGroup())) {
					// In this case, the signal must be routed into driven.node->getGroup() through an "input port"
					// of driven.node->getGroup() and we want to ensure that there are "input port signal nodes" every step of the way.

					// if we are in a special node group, move up until we are out of it, because we must not mess with those.
					NodeGroup *group = driven.node->getGroup();
					while (group->getGroupType() == NodeGroupType::SFU)
						group = group->getParent();

					if (group != node->getGroup()) {
						// Check if the driven is a signal, otherwise insert a signal in the same group
						auto *signal = dynamic_cast<Node_Signal*>(driven.node);
						if (signal == nullptr || signal->getGroup() != group) 
							signal = insertSignalNode(driven, group);

						// Walk the hierarchy upwards inserting a signal node on every step
						while (signal->getGroup()->getParent() != node->getGroup())
							signal = insertSignalNode({.node = signal, .port = 0ull}, signal->getGroup()->getParent());
					}
				}
			}
		}
	}
}


/// @details It seems many parts of the vhdl export still require signal nodes so this step adds back in missing ones
void Circuit::ensureSignalNodePlacement()
{
	utils::UnstableMap<NodePort, Node_Signal*> addedSignalsNodes;

	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();
		if (dynamic_cast<Node_Signal*>(node) != nullptr) continue;
		if (dynamic_cast<Node_MultiDriver*>(node) != nullptr) continue;
		for (auto i : utils::Range(node->getNumInputPorts())) {
			auto driver = node->getDriver(i);
			if (driver.node == nullptr) continue;
			if (node->getDriverConnType(i).isDependency()) continue;
			if (dynamic_cast<Node_Signal*>(driver.node) != nullptr) continue;
			if (driver.node->getGroup() != nullptr && driver.node->getGroup()->getGroupType() == NodeGroupType::SFU) continue;
			if (dynamic_cast<Node_MultiDriver*>(driver.node) != nullptr) continue;

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

/// @details For the vhdl export to properly infer inout ports, external nodes and io nodes must be directly connected to a multi driver node (in the same area).
/// This can be ensured by potentially duplicating multi driver nodes.
void Circuit::ensureMultiDriverNodePlacement()
{
	auto insertMultiDriver = [&](BaseNode *node, size_t inputPortIdx, size_t outputPortIdx, Node_MultiDriver *multi, size_t multiInputPort){

		// only duplicate if different groups
		if (node->getGroup() == multi->getGroup()) return;

		auto *newMulti = createNode<Node_MultiDriver>(2, multi->getOutputConnectionType(0));
		newMulti->moveToGroup(node->getGroup());
		newMulti->setName(multi->getName());

		newMulti->connectInput(0, {.node = node, .port = outputPortIdx});
		newMulti->connectInput(1, {.node = multi, .port = 0ull});

		node->connectInput(inputPortIdx, {.node=newMulti, .port=0ull});
		multi->connectInput(multiInputPort, {.node=newMulti, .port=0ull});
	};

	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();
		// For all external nodes
		if (auto *extNode = dynamic_cast<Node_External*>(node)) {
			// For all bidirectional (inout) ports
			for (auto i : utils::Range(node->getNumInputPorts())) {
				if (extNode->getInputPorts()[i].bidirPartner) {
					// Check if connected to multiDriver
					auto driver = node->getDriver(i);
					if (auto *multiDriver = dynamic_cast<Node_MultiDriver*>(driver.node)) {
						// Check if multiDriver is also driven by corresponding output
						for (const auto &np : node->getDirectlyDriven(*extNode->getInputPorts()[i].bidirPartner)) {
							if (np.node == multiDriver) {

								insertMultiDriver(node, i, *extNode->getInputPorts()[i].bidirPartner, multiDriver, np.port);

								break;
							}
						}
					}
				}
			}
		}
	}
}

void Circuit::ensureNoLiteralComparison()
{
	// At this point, there should no longer be any pure signal loops.
	std::function<bool(hlim::NodePort)> isLiteral;
	isLiteral = [&isLiteral](hlim::NodePort np)->bool {
		HCL_ASSERT(np.node != nullptr);

		if (!np.node->getName().empty()) return false;
		if (dynamic_cast<Node_Constant*>(np.node)) return true;
		if (dynamic_cast<Node_Signal*>(np.node)) return isLiteral(np.node->getDriver(0));
		return false;
	};

	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();
		if (dynamic_cast<Node_Compare*>(node) == nullptr) continue;

		if (isLiteral(node->getDriver(0)) && isLiteral(node->getDriver(1))) {
			auto *sigNode = createNode<Node_Signal>();
			sigNode->moveToGroup(node->getGroup());
			sigNode->connectInput(node->getDriver(0));
			sigNode->setName("compare_op_left");
			node->rewireInput(0, {.node = sigNode, .port = 0ull});

			dbg::log(dbg::LogMessage(node->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING 
					<< "Inserting named signal " << sigNode << " for compare node " << node << " to prevent literal on literal comparison"
			);

		}
	}
}

void Circuit::ensureChildNotReadingTristatePin()
{
	for (auto idx : utils::Range(m_nodes.size())) {
		auto node = m_nodes[idx].get();
		auto *pin = dynamic_cast<Node_Pin*>(node);

		if (pin == nullptr) continue;
		if (!pin->isInputPin() && pin->isOutputPin()) continue;

		bool anyDrivenInChild = false;
		for (const auto &driven : pin->getDirectlyDriven(0))
			if (driven.node->getGroup()->isChildOf(pin->getGroup())) {
				anyDrivenInChild = true;
				break;
			}

		if (anyDrivenInChild) {
			auto driven = pin->getDirectlyDriven(0);

			auto *sigNode = createNode<Node_Signal>();
			sigNode->moveToGroup(pin->getGroup());
			sigNode->connectInput({ .node = pin, .port = 0 });
			sigNode->setName(pin->getName());

			for (auto d : driven)
				d.node->rewireInput(d.port, {.node = sigNode, .port = 0ull});

			dbg::log(dbg::LogMessage(node->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING 
					<< "Inserting named signal " << sigNode << " after tristate pin " << pin << " for vhdl export because the pin is read in a sub entity."
			);
		}
	}
}

void Circuit::removeDisabledWritePorts(Subnet &subnet)
{
	for (unsigned i = 0; i < m_nodes.size(); i++) {
		if (!subnet.contains(m_nodes[i].get())) continue;
		bool removeNode = false;

		if (auto *memPort = dynamic_cast<Node_MemPort*>(m_nodes[i].get())) {

			if (!memPort->isReadPort()) {
				NodePort enableDriver = memPort->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable);
				if (auto *constNode = dynamic_cast<Node_Constant*>(enableDriver.node)) {
					//auto enableState = evaluateStatically(*this, enableDriver);
					auto enableState = constNode->getValue();
					HCL_ASSERT(enableState.size() == 1);
					if (enableState.get(sim::DefaultConfig::DEFINED, 0) && !enableState.get(sim::DefaultConfig::VALUE, 0)) {
						dbg::log(dbg::LogMessage(m_nodes[i].get()->getGroup()) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Removing " << m_nodes[i].get() << " because it is a write port with a constant zero write enable.");

						memPort->disconnectMemory();
						removeNode = true;
					}
				}
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

void Circuit::moveClockDriversToTop()
{
	for (auto &n : m_nodes) {
		if (auto *sig2clk = dynamic_cast<Node_Signal2Clk*>(n.get()))
			sig2clk->moveToGroup(m_root.get());
		if (auto *sig2rst = dynamic_cast<Node_Signal2Rst*>(n.get()))
			sig2rst->moveToGroup(m_root.get());
	}
}

void Circuit::optimizeSubnet(Subnet &subnet)
{
	//defaultValueResolution(*this, subnet);
	//cullUnusedNodes(subnet); // Dirty way of getting rid of default nodes
	
	propagateConstants(subnet);
	mergeRewires(subnet);
	optimizeRewireNodes(subnet);
	cullMuxConditionNegations(subnet);
	//breakMutuallyExclusiveMuxChains(subnet);
	mergeMuxes(subnet);
	removeIrrelevantComparisons(subnet);
	removeIrrelevantMuxes(subnet);
	mergeBinaryMuxChain(subnet);
	removeNoOps(subnet);
	foldRegisterMuxEnableLoops(subnet);
	removeConstSelectMuxes(subnet);
	propagateConstants(subnet); // do again after muxes are removed
	cullUnusedNodes(subnet);
}


void Circuit::postprocess(const PostProcessor &postProcessor)
{
	dbg::changeState(dbg::State::POSTPROCESS, this);
	postProcessor.run(*this);

	detectUnguardedCDCCrossings(*this, ConstSubnet::all(*this), [this](const BaseNode *affectedNode) {
		dbg::log(dbg::LogMessage(affectedNode->getGroup()) << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_POSTPROCESSING 
				<< "Unintentional clock domain crossing detected at node " << affectedNode
		);

		ConstSubnet net;
		net.add(affectedNode); 
		for(size_t i=0; i< 10; i++)
			net.dilate(false, true);
		visualize(*this, "CDC_partial", net);
		visualize(*this, "CDC_full");

		std::stringstream msg;
		msg << "Unintentional clock domain crossing detected at node: " << affectedNode->getName() << " " << affectedNode->getTypeName() << " (" << affectedNode->getId() << " ) from:"
			<< affectedNode->getStackTrace();
		HCL_DESIGNCHECK_HINT(false, msg.str());
	});

	dbg::changeState(dbg::State::POSTPROCESSINGDONE, this);

}

void DefaultPostprocessing::generalOptimization(Circuit &circuit) const
{
	circuit.insertConstUndefinedNodes();
	Subnet subnet = Subnet::all(circuit);
	circuit.disconnectZeroBitConnections();
	circuit.disconnectZeroBitOutputPins();
	defaultValueResolution(circuit, subnet);
	circuit.cullUnusedNodes(subnet); // Dirty way of getting rid of default nodes

	circuit.propagateConstants(subnet);
	circuit.ensureEntityPortSignalNodes();
	circuit.cullOrphanedSignalNodes();
	circuit.cullUnnamedSignalNodes();
	circuit.cullSequentiallyDuplicatedSignalNodes();
	subnet = Subnet::all(circuit);
	circuit.mergeRewires(subnet);
	circuit.optimizeRewireNodes(subnet);
	circuit.cullMuxConditionNegations(subnet);
	//circuit.breakMutuallyExclusiveMuxChains(subnet);
	circuit.mergeMuxes(subnet);
	circuit.removeIrrelevantComparisons(subnet);
	circuit.removeIrrelevantMuxes(subnet);	
	circuit.mergeBinaryMuxChain(subnet);
	circuit.removeNoOps(subnet);
	circuit.foldRegisterMuxEnableLoops(subnet);
	circuit.removeConstSelectMuxes(subnet);
	circuit.propagateConstants(subnet); // do again after muxes are removed
	circuit.cullUnusedNodes(subnet);
	circuit.removeDisabledWritePorts(subnet);
/*
	{
			DotExport exp("before_resolving_retiming.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("before_resolving_retiming.svg");	
	}
*/

	subnet = Subnet::all(circuit);
	determineNegativeRegisterEnables(circuit, subnet);
	/*
	{
			DotExport exp("neg_reg_enables.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("neg_reg_enables.svg");	
	}
	*/

	resolveRetimingHints(circuit, subnet);
	/*
	{
			DotExport exp("before_resolving.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("before_resolving.svg");	
	}
	*/
	annihilateNegativeRegisters(circuit, subnet);
	bypassRetimingBlockers(circuit, subnet);
/*
	{
			DotExport exp("after_general_optimization.dot");
			exp(circuit, ConstSubnet::all(circuit));
			exp.runGraphViz("after_general_optimization.svg");	
	}
*/
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
	circuit.moveClockDriversToTop();
	circuit.ensureSignalNodePlacement();
	circuit.ensureMultiDriverNodePlacement();
	circuit.ensureNoLiteralComparison();
	circuit.ensureChildNotReadingTristatePin();
	circuit.inferSignalNames();
}



void DefaultPostprocessing::run(Circuit &circuit) const
{
	dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Running default postprocessing.");

	TechnologyMapping fallbackMapping;
	const TechnologyMapping* techMapping = m_techMapping ? m_techMapping : &fallbackMapping;

	techMapping->apply(circuit, circuit.getRootNodeGroup(), true);

	generalOptimization(circuit);
	memoryDetection(circuit);

	techMapping->apply(circuit, circuit.getRootNodeGroup(), false);
	generalOptimization(circuit); // Because we ran frontend code for tech mapping

	exportPreparation(circuit);
}




void MinimalPostprocessing::generalOptimization(Circuit& circuit) const
{
	Subnet subnet = Subnet::all(circuit);
	circuit.disconnectZeroBitConnections();
	circuit.disconnectZeroBitOutputPins();
	defaultValueResolution(circuit, subnet);
	circuit.cullUnusedNodes(subnet); // Dirty way of getting rid of default nodes

	subnet = Subnet::all(circuit);
	determineNegativeRegisterEnables(circuit, subnet);
	resolveRetimingHints(circuit, subnet);
	annihilateNegativeRegisters(circuit, subnet);
	bypassRetimingBlockers(circuit, subnet);

	circuit.ensureEntityPortSignalNodes();
	circuit.cullOrphanedSignalNodes();
	circuit.cullUnnamedSignalNodes();
	subnet = Subnet::all(circuit);
	circuit.cullUnusedNodes(subnet);

	attributeFusion(circuit);
}

void MinimalPostprocessing::memoryDetection(Circuit& circuit) const
{
	findMemoryGroups(circuit);
	circuit.cullUnnamedSignalNodes();

	Subnet subnet = Subnet::all(circuit);
	circuit.cullUnusedNodes(subnet); // do again after memory group extraction with potential register retiming
}

void MinimalPostprocessing::exportPreparation(Circuit& circuit) const
{
	circuit.moveClockDriversToTop();
	circuit.ensureSignalNodePlacement();
	circuit.ensureMultiDriverNodePlacement();
	circuit.ensureNoLiteralComparison();
	circuit.ensureChildNotReadingTristatePin();
	circuit.inferSignalNames();
}



void MinimalPostprocessing::run(Circuit& circuit) const
{
	dbg::log(dbg::LogMessage() << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_POSTPROCESSING << "Running default postprocessing.");
	TechnologyMapping mapping;


	mapping.apply(circuit, circuit.getRootNodeGroup(), true);
	generalOptimization(circuit);
	memoryDetection(circuit);

	mapping.apply(circuit, circuit.getRootNodeGroup(), false);
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

std::uint64_t Circuit::allocateRevisitColor(utils::RestrictTo<RevisitCheck>)
{
	HCL_ASSERT(!m_revisitColorInUse);
	m_revisitColorInUse = true;
	return m_nextRevisitColor++;
}

void Circuit::freeRevisitColor(std::uint64_t color, utils::RestrictTo<RevisitCheck>)
{
	HCL_ASSERT(m_revisitColorInUse);
	m_revisitColorInUse = false;
}

void Circuit::setNodeId(BaseNode *node)
{
#ifdef WIN32 // TODO find a portable way to do the same on linux
	if (std::ranges::find(m_debugNodeId, m_nextNodeId) != m_debugNodeId.end())
		DebugBreak();
#endif

	node->setId(m_nextNodeId++, {});
}

void Circuit::setClockId(Clock *clock)
{
	clock->setId(m_nextClockId++, {});
}

void Circuit::readDebugNodeIds()
{
	{
		std::ifstream f{ "debug_nodes.txt" };
		while (f)
		{
			size_t id = 0;
			f >> id;
			if (id)
				m_debugNodeId.push_back(id);
		}
		std::ranges::sort(m_debugNodeId);
	}
	std::filesystem::remove("debug_nodes.txt");
}

BaseNode *Circuit::findFirstNodeByName(std::string_view name)
{
	for (auto &n : m_nodes)
		if (n->getName() == name)
			return n.get();
	return nullptr;
}

void Circuit::shuffleNodes()
{
	std::mt19937 rng;
	std::shuffle(m_nodes.begin(), m_nodes.end(), rng);
}

}
