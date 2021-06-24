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

#include "Scope.h"

#include "TechnologyMapping.h"

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <set>

namespace gtry {

NodeGroupIO::NodeGroupIO(hlim::NodeGroup *nodeGroup)
{	
	std::set<hlim::NodePort> inputs;
	std::set<hlim::NodePort> outputs;

	for (auto &n : nodeGroup->getNodes()) {
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node && driver.node->getGroup() != nodeGroup && !driver.node->getGroup()->isChildOf(nodeGroup))
				inputs.insert({.node=n, .port=i});
		}
		for (auto i : utils::Range(n->getNumOutputPorts())) {
			for (auto driven : n->getDirectlyDriven(i)) {
				if (driven.node && driven.node->getGroup() != nodeGroup && !driven.node->getGroup()->isChildOf(nodeGroup))
					outputs.insert({.node=n, .port=i});
			}
		}
	}
	
	std::set<std::string> usedNames;
	for (auto np : inputs) {
		auto *signal = dynamic_cast<hlim::Node_Signal*>(np.node);
		HCL_ASSERT_HINT(signal, "First node of signal entering node group must be a signal node at this stage!");
		HCL_ASSERT_HINT(!usedNames.contains(signal->getName()), "input-output signal name duplicates!");
		usedNames.insert(signal->getName());

		switch (signal->getOutputConnectionType(0).interpretation) {
			case hlim::ConnectionType::BITVEC: {
				inputBVecs[signal->getName()] = SignalReadPort(signal->getDriver(0));
				auto &bvec = inputBVecs[signal->getName()];
				signal->connectInput(bvec.getOutPort());
			} break;
			case hlim::ConnectionType::BOOL: {
				inputBits[signal->getName()] = SignalReadPort(signal->getDriver(0));
				auto &bit = inputBits[signal->getName()];
				signal->connectInput(bit.getOutPort());
			} break;
		}
	}
	usedNames.clear();
	for (auto np : outputs) {
		auto *signal = dynamic_cast<hlim::Node_Signal*>(np.node);
		HCL_ASSERT_HINT(signal, "Last node of signal leaving node group must be a signal node at this stage!");
		HCL_ASSERT_HINT(!usedNames.contains(signal->getName()), "input-output signal name duplicates!");
		usedNames.insert(signal->getName());

		switch (signal->getOutputConnectionType(0).interpretation) {
			case hlim::ConnectionType::BITVEC: {
				outputBVecs[signal->getName()] = BitWidth(signal->getOutputConnectionType(0).width);
				auto &bvec = outputBVecs[signal->getName()];

				while (!signal->getDirectlyDriven(0).empty()) {
					auto np = signal->getDirectlyDriven(0).front();
					np.node->rewireInput(np.port, bvec.getOutPort());
				}
				bvec = SignalReadPort(signal);
			} break;
			case hlim::ConnectionType::BOOL: {
				outputBits[signal->getName()] = {};
				auto &bit = outputBits[signal->getName()];
				while (!signal->getDirectlyDriven(0).empty()) {
					auto np = signal->getDirectlyDriven(0).front();
					np.node->rewireInput(np.port, bit.getOutPort());
				}
				bit = SignalReadPort(signal);
			} break;
		}
	}
}



TechnologyMappingPattern::TechnologyMappingPattern()
{
}


void TechnologyMapping::apply()
{
	for (auto &g : DesignScope::get()->getCircuit().getRootNodeGroup()->getChildren())
		apply(g.get());
}

void TechnologyMapping::apply(hlim::NodeGroup *nodeGroup)
{
	bool handled = false;
	{
		// put everything by default into parent group
		GroupScope scope(nodeGroup->getParent());

		for (auto &pattern : m_patterns)
			if (pattern->attemptApply(nodeGroup)) {
				handled = true;
				break;
			}

	}

	if (!handled)
		for (auto &g : nodeGroup->getChildren())
			apply(g.get());
}		


}

