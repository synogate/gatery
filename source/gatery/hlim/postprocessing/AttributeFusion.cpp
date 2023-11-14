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

#include <gatery/utils/StableContainers.h>

#include "DefaultValueResolution.h"

#include "../supportNodes/Node_Attributes.h"
#include "../coreNodes/Node_Signal.h"

#include "../Circuit.h"

#include <map>
#include <vector>

namespace gtry::hlim {

void attributeFusion(Circuit &circuit)
{
	return;
	utils::StableMap<NodePort, std::vector<std::pair<unsigned, Node_Attributes*>>> attributes;

	// find all attribs and their "distance" (order)
	for (auto &n : circuit.getNodes()) {
		if (auto *attribNode = dynamic_cast<Node_Attributes*>(n.get())) {

			unsigned distance = 0;
			NodePort np = attribNode->getDriver(0);
			while (dynamic_cast<Node_Signal*>(np.node)) {
				np = np.node->getDriver(0);
				distance++;

				HCL_ASSERT_HINT(distance < 10000, "Possible loop detected!");
			}

			attributes[np].push_back({distance, attribNode});
		}
	}

	// Sort all attribs according to their order and fuse them into the first one.
	// Move the first one to the driver, mark all others for removal
	utils::UnstableSet<BaseNode*> nodesToDelete;
	for (auto &driver : attributes) {
		std::sort(driver.second.begin(), driver.second.end(), [](const auto &lhs, const auto &rhs)->bool {
			if (lhs.first < rhs.first) return true;
			if (lhs.first > rhs.first) return false;
			return lhs.second->getId() < rhs.second->getId(); // Sort by construction order for equal distance
		});

		auto *dst = driver.second.front().second;
		for (size_t i = 1; i < driver.second.size(); i++) {
			auto *otherAttrib = driver.second[i].second;
			dst->getAttribs().fuseWith(otherAttrib->getAttribs());
			nodesToDelete.insert(otherAttrib);
		}

		auto newDriver = dst->getNonSignalDriver(0);
		dst->connectInput(newDriver);
		dst->moveToGroup(newDriver.node->getGroup());
		///@todo Push through e.g. export overrides?
	}

	// Sweep through graph and remove all now irrelevant attrib nodes
	for (size_t i = 0; i < circuit.getNodes().size(); i++) {
		if (nodesToDelete.contains(circuit.getNodes()[i].get())) {
			if (i+1 != circuit.getNodes().size())
				circuit.getNodes()[i] = std::move(circuit.getNodes().back());
			circuit.getNodes().pop_back();
			i--;
		}
	}

}

}
