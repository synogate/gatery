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


#include "../utils/ConfigTree.h"

#include "Subnet.h"

#include "supportNodes/Node_ExportOverride.h"
#include "supportNodes/Node_SignalTap.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Pin.h"


#include "Circuit.h"
#include "Node.h"
#include "NodePort.h"
#include "NodeGroup.h"
#include "coreNodes/Node_Register.h"
#include "supportNodes/Node_Memory.h"


namespace gtry::hlim {

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::all(CircuitType &circuit)
{
	FinalType res;
	res.addAll(circuit);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs)
{
	FinalType res;
	res.addAllDrivenByOutputs(outputs, limitingInputs);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs)
{
	FinalType res;
	res.addAllNecessaryForInputs(limitingOutputs, inputs);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes)
{
	FinalType res;
	res.addAllNecessaryForNodes(limitingNodes, nodes);
	return res;
}


template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs)
{
	FinalType res;
	res.addAllDrivenCombinatoricallyByOutputs(outputs);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs, bool includeRefed)
{
	FinalType res;
	res.addAllForSimulation(circuit, outputs, includeRefed);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allForSimulation(CircuitType &circuit, const utils::StableSet<hlim::NodePort> &outputs, bool includeRefed)
{
	FinalType res;
	res.addAllForSimulation(circuit, outputs, includeRefed);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allForExport(CircuitType &circuit, const utils::ConfigTree *exportSelectionConfig)
{
	FinalType res;
	res.addAllForExport(circuit, exportSelectionConfig);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::allUsedNodes(CircuitType &circuit)
{
	FinalType res;
	res.addAllUsedNodes(circuit);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType SubnetTemplate<makeConst, FinalType>::fromNodeGroup(NodeGroup* nodeGroup, bool reccursive)
{
	FinalType res;
	res.addAllFromNodeGroup(nodeGroup, reccursive);
	return res;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::add(NodeType *node)
{
	m_nodes.insert(node);
	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType& SubnetTemplate<makeConst, FinalType>::add(CircuitType& circuit, size_t nodeId)
{
	for (auto& n : circuit.getNodes())
		if (n->getId() == 609)
			add(n.get());
	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::remove(NodeType *node)
{
	m_nodes.erase(node);
	return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs)
{
	HCL_ASSERT_HINT(false, "Not yet implemented");
	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs)
{
	std::vector<NodeType*> openList;
	utils::UnstableSet<NodeType*> foundNodes;

	// Find roots
	for (auto &np : inputs) {
		auto driver = np.node->getDriver(np.port);
		if (driver.node != nullptr)
			openList.push_back(driver.node);
	}

	// Find limits
	for (auto &np : limitingOutputs)
		foundNodes.insert(np.node);

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (foundNodes.contains(n)) continue; // already handled
		foundNodes.insert(n);
		m_nodes.insert(n);
		
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node != nullptr)
				openList.push_back(driver.node);
		}
	}

	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes)
{
	std::vector<NodeType*> openList(nodes.begin(), nodes.end());
	utils::UnstableSet<NodeType*> foundNodes(limitingNodes.begin(), limitingNodes.end());

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (foundNodes.contains(n)) continue; // already handled
		foundNodes.insert(n);
		m_nodes.insert(n);
		
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node != nullptr)
				openList.push_back(driver.node);
		}
	}

	return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs)
{
	std::vector<NodeType*> openList;

	for (auto &o : outputs)
		if (o.node->isCombinatorial(o.port))
			for (auto &c : o.node->getDirectlyDriven(o.port))
				openList.push_back(c.node);

	utils::UnstableSet<NodeType*> foundNodes;

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (foundNodes.contains(n)) continue; // already handled
		foundNodes.insert(n);

		m_nodes.insert(n);
		
		for (auto i : utils::Range(n->getNumOutputPorts())) 
			if (n->isCombinatorial(i))
				for (auto &c : n->getDirectlyDriven(i))
					openList.push_back(c.node);
	}

	return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAll(CircuitType &circuit)
{
	for (auto &n : circuit.getNodes())
		m_nodes.insert(n.get());
	return (FinalType&)*this;
}

template<typename SubnetType, typename CircuitType, bool makeConst, typename NodeType, typename Container>
void addAllForSimulationImpl(SubnetType &subnet, CircuitType &circuit, const Container &outputs, bool includeRefed)
{
	std::vector<NodeType*> openList;
	utils::UnstableSet<NodeType*> handledNodes;

	// Find roots
	if (outputs.empty()) {
		for (auto &n : circuit.getNodes())
			if (n->hasSideEffects() || (includeRefed && n->hasRef()))
				openList.push_back(n.get());
	} else {
		for (auto np : outputs)
			openList.push_back(np.node);
	}

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (handledNodes.contains(n)) continue; // already handled
		handledNodes.insert(n);

		// Ignore the export-only part
		if (dynamic_cast<typename ConstAdaptor<makeConst, hlim::Node_ExportOverride>::type*>(n)) {
			if (n->getDriver(0).node != nullptr)
				openList.push_back(n->getDriver(0).node);
		} else {
			for (auto i : utils::Range(n->getNumInputPorts())) {
				auto driver = n->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver.node);
			}
		}
	}

	subnet.insert(handledNodes.anyOrder().begin(), handledNodes.anyOrder().end());
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs, bool includeRefed)
{
	addAllForSimulationImpl<SubnetTemplate<makeConst, FinalType>, CircuitType, makeConst, NodeType, std::set<hlim::NodePort>>(*this, circuit, outputs, includeRefed);

	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllForSimulation(CircuitType &circuit, const utils::StableSet<hlim::NodePort> &outputs, bool includeRefed)
{
	addAllForSimulationImpl<SubnetTemplate<makeConst, FinalType>, CircuitType, makeConst, NodeType, utils::StableSet<hlim::NodePort>>(*this, circuit, outputs, includeRefed);

	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllForExport(CircuitType &circuit, const utils::ConfigTree *exportSelectionConfig)
{
	bool includeAsserts = false;
	if (exportSelectionConfig)
		(*exportSelectionConfig)["include_asserts"].as(includeAsserts);
	bool includeSignalTaps = true;
	if (exportSelectionConfig)
		(*exportSelectionConfig)["include_taps"].as(includeSignalTaps);

	std::vector<NodeType*> openList;
	utils::StableSet<NodeType*> handledNodes;

	// Find roots
	for (auto &n : circuit.getNodes())
		if (n->hasSideEffects()) {
			if (auto sigTap = dynamic_cast<Node_SignalTap*>(n.get())) {
				if (sigTap->getLevel() == Node_SignalTap::LVL_WATCH) {
					if (!includeSignalTaps) continue;
				} else {
					if (sigTap->getTrigger() != Node_SignalTap::TRIG_FIRST_INPUT_HIGH && sigTap->getTrigger() != Node_SignalTap::TRIG_FIRST_INPUT_LOW) continue; 
					if (!includeAsserts) continue; // ... and potentially not even those.
				}
			} else if (auto *pin = dynamic_cast<Node_Pin*>(n.get())) {
				if (pin->getPinNodeParameter().simulationOnlyPin)
					continue;
			}
			openList.push_back(n.get());
		}

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (handledNodes.contains(n)) continue; // already handled
		handledNodes.insert(n);

		// Ignore the simulation-only part
		if (dynamic_cast<typename ConstAdaptor<makeConst, hlim::Node_ExportOverride>::type*>(n)) {
			if (n->getDriver(1).node != nullptr)
				openList.push_back(n->getDriver(1).node);
		} else {
			for (auto i : utils::Range(n->getNumInputPorts())) {
				auto driver = n->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver.node);
			}
		}
	}
	m_nodes.insert(handledNodes.begin(), handledNodes.end());

	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllUsedNodes(CircuitType &circuit)
{
	std::vector<NodeType*> openList;
	utils::StableSet<NodeType*> usedNodes;

	// Find roots
	for (auto &n : circuit.getNodes())
		if (n->hasSideEffects() || n->hasRef())
			openList.push_back(n.get());

	// Find dependencies
	while (!openList.empty()) {
		auto *n = openList.back();
		openList.pop_back();

		if (usedNodes.contains(n)) continue; // already handled
		usedNodes.insert(n);
		
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node != nullptr)
				openList.push_back(driver.node);
		}
	}

	m_nodes.insert(usedNodes.begin(), usedNodes.end());

	return (FinalType&)*this;
}

template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addDrivenNamedSignals(CircuitType &circuit)
{
	// Loop over all nodes and if they are named signal nodes, check if their driver is part of the subnet.
	// If so, add the named signal node and its path to the subnet.
	for (auto &n : circuit.getNodes()) {
		if (auto *sigNode = dynamic_cast<typename ConstAdaptor<makeConst, hlim::Node_Signal>::type*>(n.get())) {
			if (sigNode->hasGivenName()) {
				auto finalDriver = sigNode->getNonSignalDriver(0);
				if (contains(finalDriver.node)) {
					// add entire path
					NodeType *node = sigNode;
					while (node != finalDriver.node) {
						m_nodes.insert(node);
						node = node->getDriver(0).node;
					}
				}
			}
		}
	}

	return (FinalType&)*this;
}



template<bool makeConst, typename FinalType>
FinalType &SubnetTemplate<makeConst, FinalType>::addAllFromNodeGroup(NodeGroup* nodeGroup, bool reccursive)
{
	m_nodes.insert(nodeGroup->getNodes().begin(), nodeGroup->getNodes().end());
	if (reccursive)
		for (auto& c : nodeGroup->getChildren())
			addAllFromNodeGroup(c.get(), reccursive);

	return (FinalType&)*this;
}


template<bool makeConst, typename FinalType>
void SubnetTemplate<makeConst, FinalType>::dilate(bool forward, bool backward)
{
	DilateDir dir = DilateDir::none;
	if (forward)
		dir = DilateDir::output;
	if (backward)
		dir = DilateDir::input;
	if (forward && backward)
		dir = DilateDir::both;

	dilate(dir, 1);
}

template<bool makeConst, typename FinalType>
void SubnetTemplate<makeConst, FinalType>::dilate(DilateDir dir, size_t steps, std::optional<NodeType*> startNode)
{
	dilateIf([=](const NodeType& node) {
		return dir;
	}, steps, startNode);
}

template<bool makeConst, typename FinalType>
void SubnetTemplate<makeConst, FinalType>::dilateIf(std::function<DilateDir(const NodeType&)> filter, size_t stepLimit, std::optional<NodeType*> startNode)
{
	size_t steps = 0;
	std::vector<NodeType*> newNodes;
	std::vector<NodeType*> lastStepNodes;

	if (startNode)
	{
		add(*startNode);
		lastStepNodes.push_back(*startNode);
	}
	else
		lastStepNodes.insert(lastStepNodes.begin(), m_nodes.begin(), m_nodes.end());

	do
	{
		newNodes.clear();

		for (NodeType* n : lastStepNodes)
		{
			DilateDir dir = filter(*n);

			if (dir == DilateDir::input || dir == DilateDir::both)
				for (auto i : utils::Range(n->getNumInputPorts())) 
					if(auto np = n->getDriver(i); np.node)
						if (auto [it, inserted] = m_nodes.insert(np.node); inserted)
							newNodes.push_back(np.node);

			if (dir == DilateDir::output || dir == DilateDir::both)
				for (auto i : utils::Range(n->getNumOutputPorts())) 
					for (auto np : n->getDirectlyDriven(i))
						if (auto [it, inserted] = m_nodes.insert(np.node); inserted)
							newNodes.push_back(np.node);
		}
		lastStepNodes.swap(newNodes);

	} while (++steps != stepLimit && !newNodes.empty());
}

template<bool makeConst, typename FinalType>
FinalType gtry::hlim::SubnetTemplate<makeConst, FinalType>::filterLoopNodesOnly() const
{
	FinalType ret;

	auto isPartOfLoop = [](NodeType* node)
	{
		for (auto i : utils::Range(node->getNumOutputPorts()))
		{
			// MiO: did not exclude registers and memories
			//if (!node->isCombinatorial(i))
			//	continue;

			utils::UnstableSet<NodeType*> seen;
			for (auto&& nh : ((BaseNode*)node)->exploreOutput(i))
			{
				if (auto drivingOutput = nh.node()->getDriver(nh.port()); drivingOutput.node->getOutputType(drivingOutput.port) != hlim::NodeIO::OUTPUT_IMMEDIATE)
					nh.backtrack();
				else if (nh.template isNodeType<Node_Register>())
					nh.backtrack();
				else if (nh.template isNodeType<Node_Memory>())
					nh.backtrack();
				else if (seen.contains(nh.node()))
					nh.backtrack();
				else if (nh.node() == node)
					return true;
				else
					seen.insert(nh.node());
			}
		}
		return false;
	};

	for (auto start : m_nodes)
		if (isPartOfLoop(start))
			ret.add(start);
	
	return ret;
}

template class SubnetTemplate<false, Subnet>;
template class SubnetTemplate<true, ConstSubnet>;


}
