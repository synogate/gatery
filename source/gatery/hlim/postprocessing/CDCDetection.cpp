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

#include "CDCDetection.h"

#include "../Circuit.h"
#include "../Subnet.h"
#include "../Node.h"
#include "../../debug/DebugInterface.h"
#include <gatery/utils/Range.h>

namespace gtry::hlim {

void inferClockDomains(Circuit &circuit, utils::UnstableMap<hlim::NodePort, SignalClockDomain> &domains)
{
	domains.clear();

	/*
		Determine clock domains by looking at all nodes in sequence. For some, the clock domain of the outputs can be determined (constants, pins, registers),
		for others it is dependent on inputs. For the latter, we build a tree-like dependency structure in the "undetermined" map.
		Whenever a new output is assigned to a clock domain, this map is accessed and if a subtree is dependent on this output it is recursively rechecked.
	*/
	 
	utils::UnstableMap<NodePort, std::vector<NodePort>> undetermined;


	auto assignToCD = [&](const NodePort &np, SignalClockDomain cd, utils::StableSet<NodePort> &nodePortsToRetry){
		if (domains.contains(np)) return;
		domains[np] = cd;

		auto it = undetermined.find(np);
		if (it != undetermined.end())
			for (auto n : it->second)
				nodePortsToRetry.insert(n);
				//nodePortsToRetry.push_back(n);
	};

	auto attemptResolve = [&](const NodePort &nodePort) {

		// Only do this the first time
		bool insertIntoUndetermined = true;

		//std::vector<NodePort> nodePortsToRetry;
		//nodePortsToRetry.push_back(nodePort);
		utils::StableSet<NodePort> nodePortsToRetry;
		nodePortsToRetry.insert(nodePort);

		while (!nodePortsToRetry.empty()) {
			// auto np = nodePortsToRetry.back();
			// nodePortsToRetry.pop_back();

			auto np = *nodePortsToRetry.begin();
			nodePortsToRetry.erase(nodePortsToRetry.begin());

			auto ocr = np.node->getOutputClockRelation(np.port);
			if (ocr.isConst())
				assignToCD(np, { .type = SignalClockDomain::CONSTANT }, nodePortsToRetry);
			else {
				if (!ocr.dependentClocks.empty()) {
					if (ocr.dependentClocks[0] == nullptr)
						assignToCD(np, { .type = SignalClockDomain::UNKNOWN }, nodePortsToRetry);
					else
						assignToCD(np, { .type = SignalClockDomain::CLOCK, .clk = ocr.dependentClocks[0] }, nodePortsToRetry);
				}
				else {
					bool allConst = true;
					for (auto i : ocr.dependentInputs) {
						auto driver = np.node->getDriver(i);
						if (driver.node == nullptr) continue;

						auto it = domains.find(driver);
						if (it != domains.end()) {
							switch (it->second.type) {
							case SignalClockDomain::CONSTANT:
								break;
							case SignalClockDomain::UNKNOWN:
							case SignalClockDomain::CLOCK:
								assignToCD(np, it->second, nodePortsToRetry);
								allConst = false;
								break;
							}
						}
						else {
							allConst = false;
							if (insertIntoUndetermined)
								undetermined[driver].push_back(np);
						}
					}

					if (allConst)
						assignToCD(np, { .type = SignalClockDomain::CONSTANT }, nodePortsToRetry);
				}
			}
		}
	};

	for (auto &n : circuit.getNodes()) {
		for (auto i : utils::Range(n->getNumOutputPorts())) {
			NodePort np = {.node = n.get(), .port = i};
			attemptResolve(np);
		}
	}
}


void detectUnguardedCDCCrossings(Circuit &circuit, const ConstSubnet &subnet, std::function<void(const BaseNode*)> detectionCallback)
{
	utils::UnstableMap<hlim::NodePort, SignalClockDomain> domains;
	inferClockDomains(circuit, domains);
	std::vector<SignalClockDomain> inputClocks;
	for (const auto &n : subnet) {
		inputClocks.resize(n->getNumInputPorts());
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (driver.node == nullptr) 
				inputClocks[i] = {.type = SignalClockDomain::CONSTANT};
			else {
				auto it = domains.find(driver);
				if (it != domains.end())
					inputClocks[i] = it->second;
				else
					inputClocks[i] = {.type = SignalClockDomain::CONSTANT};
			}
		}

		if (!n->checkValidInputClocks(inputClocks))
			detectionCallback(n);
	}
}

}
