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
#include "NodeCategorization.h"

#include "Node.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Register.h"

#include "../utils/Range.h"

#include <stack>

namespace mhdl {
namespace core {
namespace hlim {

#if 0
void NodeCategorization::parse(NodeGroup *group)
{
	// Find all connecting signals (inputs, outputs, child inputs, child outputs)
	for (auto node : group->getNodes()) {
		for (auto i : utils::Range(node->getNumInputPorts())) {
			auto driver = node->getDriver(i);

			if (driver.node == nullptr)
				unconnected.insert(node);
			else {
				if (driver.node->getGroup()->isChildOf(group)) { // Either it is being driven by a child module...
					childOutputSignals.insert(
				} else {										 // .. or by an outside node.
					Node_Signal *signal = dynamic_cast<Node_Signal*>(drivingNode);
					HCL_ASSERT(signal != nullptr);
					inputSignals[signal] = drivingNode->getGroup();
				}
			}
		}
		
		for (auto i : utils::Range(node->getNumOutputs())) {
			for (auto &connection : node->getOutput(i).connections) {
				Node *drivenNode = connection->node;
				
				if (!isNodeInConsideredSet(drivenNode)) {
					if (drivenNode->getGroup()->isChildOf(group)) {
						Node_Signal *signal = dynamic_cast<Node_Signal *>(node);
						if (signal != nullptr) {
							childInputSignals.insert({signal, drivenNode->getGroup()});
						} else {
							Node_Signal *signal = dynamic_cast<Node_Signal *>(drivenNode);
							HCL_ASSERT(signal != nullptr);
							childInputSignals.insert({signal, drivenNode->getGroup()});
						}
					} else {
						Node_Signal *signal = dynamic_cast<Node_Signal *>(node);
						if (signal != nullptr) {
							outputSignals.insert({signal, drivenNode->getGroup()});
						} else {
							Node_Signal *signal = dynamic_cast<Node_Signal *>(drivenNode);
							HCL_ASSERT(signal != nullptr);
							outputSignals.insert({signal, drivenNode->getGroup()});
						}
					}
				}
			}
		}
	}
	
	
	// Trace from output signals backwards and categorize
	{
		utils::UnstableSet<Node_Signal*> closedList;
		utils::StableSet<Node_Signal*> openList;
		for (auto signal : outputSignals)
			openList.insert(signal.first);
		for (auto signal : childInputSignals)
			openList.insert(signal.first);

		for (auto signal : inputSignals)
			closedList.insert(signal.first);
		for (auto signal : childOutputSignals)
			closedList.insert(signal.first);
		
		while (!openList.empty()) {
			Node_Signal* signal = *openList.begin();
			openList.erase(openList.begin());
			closedList.insert(signal);

			//HCL_DESIGNCHECK_HINT(signal->getInput(0).connection != nullptr, "Undriven signal used to compose outputs!");
			if (signal->getInput(0).connection == nullptr) {
				unconnected.insert(signal);
				continue;
			}
			
			hlim::Node *driver = signal->getInput(0).connection->node;		
			
			if (dynamic_cast<hlim::Node_Signal*>(driver) != nullptr) {
				auto driverSignal = (hlim::Node_Signal*)driver;
				
				if (closedList.find(driverSignal) != closedList.end()) continue;	
				internalSignals.insert(driverSignal);
				openList.insert(driverSignal);
			} else {
				if (dynamic_cast<hlim::Node_Register*>(driver) != nullptr) {
					registers.insert((hlim::Node_Register*)driver);
				} else {
					combinatorial.insert(driver);
				}
				
				for (auto i : utils::Range(driver->getNumInputs())) {
					auto driverSignal = dynamic_cast<hlim::Node_Signal*>(driver->getInput(i).connection->node);
					HCL_ASSERT(driverSignal);
					if (closedList.find(driverSignal) != closedList.end()) continue;
					
					internalSignals.insert(driverSignal);
					openList.insert(driverSignal);
				}
			}
		}
	}
	
	// Everything not touched is not used
	for (auto node : allConsideredNodes) {
		if (inputSignals.find((Node_Signal*) node) != inputSignals.end()) continue;
		if (outputSignals.find((Node_Signal*) node) != outputSignals.end()) continue;
		if (childInputSignals.find((Node_Signal*) node) != childInputSignals.end()) continue;
		if (childOutputSignals.find((Node_Signal*) node) != childOutputSignals.end()) continue;
		if (internalSignals.find((Node_Signal*) node) != internalSignals.end()) continue;
		if (registers.find((Node_Register*) node) != registers.end()) continue;
		if (combinatorial.find(node) != combinatorial.end()) continue;

		unused.insert(node);
	}
	*/
}
#endif

}
}
}
