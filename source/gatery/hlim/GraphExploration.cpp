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
#include "GraphExploration.h"

#include "coreNodes/Node_Signal.h"
#include "supportNodes/Node_ExportOverride.h"

#include "../utils/Range.h"


namespace gtry::hlim {

template<bool forward, typename Policy>
Exploration<forward, Policy>::NodePortHandle::NodePortHandle(iterator &iterator, NodePort nodePort) : m_iterator(iterator), m_nodePort(nodePort) { }

template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isSignal() const
{
	return dynamic_cast<const Node_Signal*>(node()) != nullptr;
}


template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isBranchingForward()
{
	size_t numConsumers = 0;
	for (auto i : utils::Range(node()->getNumOutputPorts())) {
		numConsumers += node()->getDirectlyDriven(i).size();
		if (numConsumers > 1) return true;
	}
	return false;
}

template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isBranchingBackward()
{
	size_t numDrivers = 0;
	for (auto i : utils::Range(node()->getNumInputPorts())) {
		if (node()->getDriver(i).node != nullptr) {
			numDrivers++;
			if (numDrivers > 1) return true;
		}
	}
	return false;
}

template<bool forward, typename Policy>
void Exploration<forward, Policy>::NodePortHandle::backtrack()
{
	m_iterator.backtrack();
}





template<bool forward, typename Policy>
Exploration<forward, Policy>::Exploration(NodePort nodePort) : m_nodePort(nodePort)
{
}

template<bool forward, typename Policy>
typename Exploration<forward, Policy>::iterator Exploration<forward, Policy>::begin()
{
	return iterator(m_skipDependencies, m_skipExportOnly, m_nodePort);
}

template<bool forward, typename Policy>
typename Exploration<forward, Policy>::iterator Exploration<forward, Policy>::end()
{
	return iterator();
}


template<bool forward>
void DepthFirstPolicy<forward>::init(NodePort nodePort)
{
	if (forward) {
		if (nodePort.node != nullptr)
			for (auto np : nodePort.node->getDirectlyDriven(nodePort.port))
				m_stack.push(np);
	} else {
		if (nodePort.node != nullptr) {
			auto driver = nodePort.node->getDriver(nodePort.port);
			if (driver.node != nullptr)
				m_stack.push(driver);
		}
	}
}

template<bool forward>
void DepthFirstPolicy<forward>::advance(bool skipDependencies, bool skipExportOnly)
{
	if (m_stack.empty()) return;
	BaseNode *currentNode = m_stack.top().node;
	m_stack.pop();
	if (forward) {
		for (auto i : utils::Range(currentNode->getNumOutputPorts()))
			if (!skipDependencies ||  currentNode->getOutputConnectionType(i).type != ConnectionType::DEPENDENCY)
				for (auto np : currentNode->getDirectlyDriven(i))
					m_stack.push(np);
	} else {
		bool isExpOverride = dynamic_cast<Node_ExportOverride*>(currentNode);
		for (auto i : utils::Range(currentNode->getNumInputPorts())) {
			auto driver = currentNode->getDriver(i);
			if (driver.node != nullptr)
				if (!skipDependencies || !hlim::outputIsDependency(driver))
					if (!skipExportOnly || isExpOverride || i != Node_ExportOverride::EXP_INPUT)
						m_stack.push(driver);
		}
	}
}

template<bool forward>
void DepthFirstPolicy<forward>::backtrack()
{
	HCL_ASSERT(!m_stack.empty());
	m_stack.pop();
}

template<bool forward>
bool DepthFirstPolicy<forward>::done() const
{
	return m_stack.empty();
}

template<bool forward>
NodePort DepthFirstPolicy<forward>::getCurrent()
{
	return m_stack.top();
}




template class DepthFirstPolicy<true>;
template class DepthFirstPolicy<false>;
template class Exploration<true, DepthFirstPolicy<true>>;
template class Exploration<false, DepthFirstPolicy<false>>;






DijkstraExploreNodesForward::iterator::iterator(const std::vector<OpenNode> &start) : BaseIterator(start) 
{ 
	uncoverNext();
}

DijkstraExploreNodesForward::iterator &DijkstraExploreNodesForward::iterator::operator++() 
{
	if (m_autoProceed)
		proceed(0, ~0ull);
	uncoverNext();
	return *this; 
}

void DijkstraExploreNodesForward::iterator::proceed(size_t cost, std::uint64_t proceedPortMask) 
{
	m_autoProceed = false;
	auto newDistance = m_current.distance + cost;

	for (auto i : utils::Range(m_current.nodePort.node->getNumOutputPorts()))
		if (proceedPortMask & (1ull << i))
			for (auto &consumer : m_current.nodePort.node->getDirectlyDriven(i))
				m_openList.push({.distance = newDistance, .nodePort = consumer});
}

bool DijkstraExploreNodesForward::iterator::operator!=(const iterator &rhs) const 
{ 
	HCL_ASSERT(rhs.m_current.nodePort.node == nullptr); 
	return m_current.nodePort.node != nullptr;
}

void DijkstraExploreNodesForward::iterator::skip() 
{
	m_autoProceed = false;
}

void DijkstraExploreNodesForward::iterator::uncoverNext()
{
	while (!m_openList.empty() && m_closedList.contains(m_openList.top().nodePort.node))
		m_openList.pop();
	
	if (!m_openList.empty()) {
		m_current = m_openList.top();
		m_openList.pop();
		m_closedList.insert(m_current.nodePort.node);
	} else {
		m_current = {};
	}

	m_autoProceed = true;
}


void DijkstraExploreNodesForward::addInputPort(NodePort inputPort) 
{
	m_start.push_back({.distance = 0, .nodePort = inputPort});
}

void DijkstraExploreNodesForward::addOutputPort(NodePort outputPort) 
{
	for (auto consumer : outputPort.node->getDirectlyDriven(outputPort.port))
		m_start.push_back({.distance = 0, .nodePort = consumer});
}

void DijkstraExploreNodesForward::addAllOutputPorts(BaseNode *node) 
{
	for (auto i : utils::Range(node->getNumOutputPorts()))
		addOutputPort({.node = node, .port = i});
}

}
