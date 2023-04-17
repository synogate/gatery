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

#include "TopologicalSort.h"

#include "Subnet.h"

#include "Circuit.h"
#include "Node.h"
#include "NodePort.h"


#include <gatery/utils/Range.h>

namespace gtry::hlim {

const std::vector<BaseNode*> &TopologicalSort::sort(const Subnet &subnet, LoopHandling loopHandling)
{
	m_sortedNodes.clear();
	m_unsortedNodes = subnet;

	utils::UnstableSet<NodePort> outputsReady;

	// Checks if all inputs of a node are ready (either because they are bound to a registered output, unconnected, bound to a node outside of the subset, or their input simply is ready)
	auto allInputsReady = [&outputsReady, &subnet](BaseNode *node)->bool {
		for (auto i : utils::Range(node->getNumInputPorts())) {
			auto driver = node->getDriver(i);
			if (driver.node == nullptr) continue; // ready
			if (!subnet.contains(driver.node)) continue; // ready

			auto outType = driver.node->getOutputType(driver.port);
			if (outType == NodeIO::OUTPUT_LATCHED) continue; // ready

			if (outputsReady.contains(driver)) continue; // ready

			// not ready
			return false;
		}
		return true;
	};
	
	// Add all initially ready nodes to stack
	std::vector<hlim::BaseNode*> nodesReady;
	for (auto &node : subnet) {
		if (allInputsReady(node))
			nodesReady.push_back(node);
	}

	while (true) {

		// Add ready nodes to list, then explore their outputs if any of the driven nodes are now also ready.
		while (!nodesReady.empty()) {
			auto *node = nodesReady.back();
			nodesReady.pop_back();
			if (!m_unsortedNodes.contains(node)) continue; // already handled
			m_unsortedNodes.erase(node);
			m_sortedNodes.push_back(node);

			for (auto i : utils::Range(node->getNumOutputPorts()))
				outputsReady.insert({.node=node, .port=i});

			for (auto i : utils::Range(node->getNumOutputPorts()))
				for (auto np : node->getDirectlyDriven(i))
					if (allInputsReady(np.node))
						nodesReady.push_back(np.node);
		}

		if (m_unsortedNodes.empty() || (loopHandling == SET_LOOPS_ASIDE)) break;

		HCL_ASSERT_HINT(loopHandling != LOOPS_ARE_ERRORS, "Can't sort topologically, subnet contains loops, " + std::to_string(m_unsortedNodes.size()) + " nodes remaining");

		// Otherwise, split the loop by declaring a node ready even though it isn't.
		// Choose the node on the loop with the lowest id.
		auto loop = getLoop();
		auto *nodeToSplit = *loop.begin();
		for (auto n : loop) 
			if (n->getId() < nodeToSplit->getId())
				nodeToSplit = n;

		nodesReady.push_back(nodeToSplit);
	}	


	return m_sortedNodes;
}


utils::StableSet<BaseNode*> TopologicalSort::getLoop()
{
	// A bit hacked: Start with all unsorted nodes and peel away everything that doesn't drive a node from the set
	// Could be done better

	utils::StableSet<hlim::BaseNode*> loopNodes(m_unsortedNodes.begin(), m_unsortedNodes.end());
	while (true) {
		utils::StableSet<hlim::BaseNode*> tmp = std::move(loopNodes);
		loopNodes.clear();

		bool done = true;
		for (auto* n : tmp) {
			bool anyDrivenInLoop = false;
			for (auto i : utils::Range(n->getNumOutputPorts()))
				for (auto nh : n->exploreOutput(i)) {
					if (!nh.isSignal()) {
						if (tmp.contains(nh.node())) {
							anyDrivenInLoop = true;
							break;
						}
						nh.backtrack();
					}
				}

			if (anyDrivenInLoop)
				loopNodes.insert(n);
			else
				done = false;
		}

		if (done) break;
	}

	return loopNodes;
}

}