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

#include "../Circuit.h"
#include "../RegisterRetiming.h"

#include <queue>
#include <limits>

namespace gtry::hlim {

/**
 * @brief Finds and returns all Node_RegHints sorted by distance (in Node_RegHint on the way) to Node_RegSpawner nodes.
 * @details Search is a forward-only dijkstra search from the Node_RegSpawner and confined to the given subnet.
 * @param spawners Where to start
 * @param subnet Confines the search
 * @return std::vector<std::pair<size_t, Node_RegHint*>> Sorted list of found Node_RegHints with their distance (as in number of Node_RegHints on the way) to the nearest Node_RegSpawner.
 */
std::vector<std::pair<size_t, Node_RegHint*>> getRegHintsSortedByDistanceToSpawners(std::span<Node_RegSpawner*> spawners, const Subnet &subnet)
{
    struct OpenNode {
        std::size_t regStages = (std::size_t) ~0ull;
        BaseNode *node;
    };

    struct NearestFirst {
        bool operator()(const OpenNode &lhs, const OpenNode &rhs) const {
            if (lhs.regStages > rhs.regStages) return true;
            if (lhs.regStages < rhs.regStages) return false;
            return lhs.node->getId() > rhs.node->getId();
        }
    };

    Subnet closedList;
    std::priority_queue<OpenNode, std::vector<OpenNode>, NearestFirst> openList;

    // Populate open list with spawners
    for (auto spawner : spawners)
        openList.push({.regStages = 0, .node = spawner});

    std::vector<std::pair<size_t, Node_RegHint*>> result;

    while (!openList.empty()) {
        // Fetch closest node (in Node_RegHint hops)
        auto top = openList.top();
        openList.pop();
        
        // Check (and discard) if already handled
        if (closedList.contains(top.node)) continue;
        closedList.add(top.node);

        // If running across a Node_RegHint
        auto stages = top.regStages;
        if (auto *regHint = dynamic_cast<Node_RegHint*>(top.node)) {
            // Add it to the result list
            result.emplace_back(stages, regHint);
            // Increment "distance" for everything that is found through this node
            stages++;
        }
        
        // Explore outputs of node by adding them to open list unless they leave given subnet
        for (auto i : utils::Range(top.node->getNumOutputPorts()))
            for (auto consumer : top.node->getDirectlyDriven(i))
                if (subnet.contains(consumer.node))
                    openList.push({.regStages = stages, .node = consumer.node});
    }

    // Actual sorting of result
    std::sort(result.begin(), result.end());
    return result;
}


void resolveRetimingHints(Circuit &circuit, Subnet &subnet)
{
    std::vector<Node_RegSpawner*> spawner;
    for (auto &n : subnet)
        if (auto *regSpawner = dynamic_cast<Node_RegSpawner*>(n))
            spawner.push_back(regSpawner);

    auto sortedRegHints = getRegHintsSortedByDistanceToSpawners(spawner, subnet);

    for (std::size_t i = sortedRegHints.size()-1; i < sortedRegHints.size(); i--) {
        auto node = sortedRegHints[i].second;

        retimeForwardToOutput(circuit, subnet, {.node = node, .port = 0}, {.downstreamDisableForwardRT = true});
        node->bypassOutputToInput(0, 0);
    }
}


void bypassRegSpawners(Circuit &circuit)
{
    for (auto &n : circuit.getNodes()) {
        if (auto *regSpawner = dynamic_cast<Node_RegSpawner*>(n.get()))
			regSpawner->bypass();
    }
}

}