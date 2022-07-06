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
#include "CDCDetection.h"

#include "../Circuit.h"
#include "../Subnet.h"
#include "../Node.h"
#include "../../debug/DebugInterface.h"

namespace gtry::hlim {

void inferClockDomains(Circuit &circuit, std::map<hlim::NodePort, SignalClockDomain> &domains)
{
	domains.clear();

	/*
		Determine clock domains by looking at all nodes in sequence. For some, the clock domain of the outputs can be determined (constants, pins, registers),
		for others it is dependent on inputs. For the latter, we build a tree-like dependency structure in the "undetermined" map.
		Whenever a new output is assigned to a clock domain, this map is accessed and if a subtree is dependent on this output it is recursively rechecked.
	*/
	 
	std::map<NodePort, std::vector<NodePort>> undetermined;

	std::function<void(const NodePort&, bool)> attemptResolve;
	std::function<void(const NodePort&, SignalClockDomain)> assignToCD;

	assignToCD = [&](const NodePort &np, SignalClockDomain cd){
		if (domains.contains(np)) return;
		domains[np] = cd;

		auto it = undetermined.find(np);
		if (it != undetermined.end())
			for (auto n : it->second)
				attemptResolve(n, false);
	};

	attemptResolve = [&](const NodePort &np, bool insertIntoUndetermined){
		auto ocr = np.node->getOutputClockRelation(np.port);
		if (ocr.isConst()) 
			assignToCD(np, {.type = SignalClockDomain::CONSTANT });
		else {
			if (!ocr.dependentClocks.empty()) {
				if (np.node->getClocks()[ocr.dependentClocks[0]] == nullptr)
					assignToCD(np, {.type = SignalClockDomain::UNKNOWN });
				else
					assignToCD(np, {.type = SignalClockDomain::CLOCK, .clk=np.node->getClocks()[ocr.dependentClocks[0]] });
			} else {
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
								assignToCD(np, it->second);
								allConst = false;
							break;
						}
					} else {
						allConst = false;
						if (insertIntoUndetermined)
							undetermined[driver].push_back(np);
					}
				}

				if (allConst)
					assignToCD(np, {.type = SignalClockDomain::CONSTANT });
			}
		}	
	};

	for (auto &n : circuit.getNodes()) {
		for (auto i : utils::Range(n->getNumOutputPorts())) {
			NodePort np = {.node = n.get(), .port = i};
			attemptResolve(np, true);
		}
	}
}


void detectUnguardedCDCCrossings(Circuit &circuit, const ConstSubnet &subnet, std::function<void(const BaseNode*, size_t)> detectionCallback)
{
	std::map<hlim::NodePort, SignalClockDomain> domains;
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
					inputClocks[i] = {.type = SignalClockDomain::UNKNOWN};
			}
		}

		for (auto i : utils::Range(n->getNumOutputPorts())) {
			auto ocr = n->getOutputClockRelation(i);

			Clock *clock = nullptr;
			size_t numUnknowns = 0;

			for (auto c : ocr.dependentClocks) {
				if (n->getClocks()[c] != nullptr) {
					if (clock == nullptr)
						clock = n->getClocks()[c]->getClockPinSource();
					else
						if (clock != n->getClocks()[c]->getClockPinSource()) 
							detectionCallback(n, i);
				}
			}
			for (auto j : ocr.dependentInputs) {
				switch (inputClocks[j].type) {
					case SignalClockDomain::CONSTANT:
					break;
					case SignalClockDomain::UNKNOWN:
						numUnknowns++;
					break;
					case SignalClockDomain::CLOCK:
						if (clock == nullptr)
							clock = inputClocks[j].clk->getClockPinSource();
						else
							if (clock != inputClocks[j].clk->getClockPinSource()) 
								detectionCallback(n, i);
					break;
				}
			}
			if (numUnknowns > 1 || (numUnknowns > 0 && clock != nullptr))
				detectionCallback(n, i);
		}
	}
}

}