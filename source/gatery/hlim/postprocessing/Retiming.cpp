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

#include "OverrideResolution.h"

#include "../supportNodes/Node_RegSpawner.h"
#include "../supportNodes/Node_RegHint.h"
#include "../supportNodes/Node_RetimingBlocker.h"

#include "../Circuit.h"
#include "../RegisterRetiming.h"
#include "../GraphTools.h"

#include "../../export/DotExport.h"

#include <queue>
#include <limits>
#include <iostream>

namespace gtry::hlim {


void resolveRetimingHints(Circuit &circuit, Subnet &subnet)
{
	// Locate all spawners in subnet
	std::vector<Node_RegSpawner*> spawner;
	for (auto &n : subnet)
		if (auto *regSpawner = dynamic_cast<Node_RegSpawner*>(n))
			spawner.push_back(regSpawner);

	// Locate all register hints in subnet that can be reached from the spawners
	auto regHints = getRegHintDistanceToSpawners(spawner, subnet);
	std::sort(regHints.begin(), regHints.end(), [](const auto &lhs, const auto &rhs){ 
		if (lhs.first < rhs.first) return true;
		if (lhs.first > rhs.first) return false;
		return lhs.second->getId() > rhs.second->getId();
	});
	/**
	 * @todo This sorting is actually not sufficient. They need to be properly topologically sorted (with the added difficult that the graph can be cyclic).
	 * For now, we get around this by disabling forward retiming for downstream registers, but this is not a good solution.
	 */

	// Resolve all register hints back to front
	for (std::size_t i = regHints.size()-1; i < regHints.size(); i--) {
		auto node = regHints[i].second;
		// Skip orphaned retiming hints
		if (node->getDirectlyDriven(0).empty()) continue;

/*
		{
			DotExport exp("state.dot");
			exp(circuit);
			exp.runGraphViz("state.svg");

			std::cout << "Retiming node " << node->getId() << std::endl;
		}
		{
			DotExport exp("subnet.dot");
			exp(circuit, subnet.asConst());
			exp.runGraphViz("subnet.svg");

			std::cout << "Retiming node " << node->getId() << std::endl;
		}
*/
		retimeForwardToOutput(circuit, subnet, {.node = node, .port = 0}, {.downstreamDisableForwardRT = true});
		node->bypassOutputToInput(0, 0);
	}

	for (auto *regSpawner : spawner)
		regSpawner->markResolved();

}

void bypassRetimingBlockers(Circuit &circuit, Subnet &subnet)
{
	for (auto &n : subnet)
		if (dynamic_cast<Node_RetimingBlocker*>(n))
			n->bypassOutputToInput(0, 0);
}

}