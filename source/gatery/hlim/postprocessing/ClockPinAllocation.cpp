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

#include "ClockPinAllocation.h"

#include "../Circuit.h"
#include "../NodePort.h"
#include "../Clock.h"
#include "../Subnet.h"

#include <queue>

namespace gtry::hlim {

std::vector<Clock*> determineRelevantClocks(Circuit &circuit, const Subnet &subnet)
{
	utils::StableMap<Clock*, bool> relevance;

	std::queue<Clock*> openList;
	// Add leaf clocks to open list
	for (auto &clock : circuit.getClocks())
		if (clock->getDerivedClocks().empty())
			openList.push(clock.get());

	// Process clocks in open list, skipping (reinserting) those whose children have not yet been evaluated.
	// Mark clocks as relevant if they are driving nodes or one of their derived clocks is relevant.
	while (!openList.empty()) {
		Clock *clock = openList.front();
		openList.pop();
		
		bool reevaluateLater = false;

		bool drivesRelevantNodes = false;
		for (auto n : clock->getClockedNodes())
			if (subnet.contains(n.node)) {
				drivesRelevantNodes = true;
				break;
			}

		bool isRelevant;

		if (drivesRelevantNodes) {
			isRelevant = true;
			reevaluateLater = false;
		} else {
			isRelevant = false;
			bool anyNotReady = false;
			for (auto child : clock->getDerivedClocks()) {
				auto it = relevance.find(child);
				if (it != relevance.end()) {
					if (it->second) {
						isRelevant = true;
						break;
					}
				} else {
					anyNotReady = true;
				}
			}

			if (!isRelevant && anyNotReady)
				reevaluateLater = true;
		}

		if (reevaluateLater) {
			openList.push(clock);
		} else {
			relevance[clock] = isRelevant;
			if (clock->getParentClock() != nullptr)
				openList.push(clock->getParentClock());
		}
	}

	std::vector<Clock*> res;
	for (auto p : relevance)
		if (p.second)
			res.push_back(p.first);

	return res;
}


ClockPinAllocation extractClockPins(Circuit &circuit, const Subnet &subnet)
{
	auto relevantClocks = determineRelevantClocks(circuit, subnet);

	ClockPinAllocation res;
	for (auto clock : relevantClocks) {
		{
			Clock *clockPin = clock->getClockPinSource();
			size_t idx;
			auto it = res.clock2ClockPinIdx.find(clockPin);
			if (it != res.clock2ClockPinIdx.end()) {
				idx = it->second;
			} else {
				idx = res.clockPins.size();
				res.clockPins.push_back({});
				res.clock2ClockPinIdx[clockPin] = idx;
			}
			res.clockPins[idx].clocks.push_back(clock);
			res.clock2ClockPinIdx[clock] = idx;

			if (clockPin == clock)
				res.clockPins[idx].source = clock;
		}

		{
			Clock *resetPin = clock->getResetPinSource();
			if (resetPin != nullptr) {
				size_t idx;
				auto it = res.clock2ResetPinIdx.find(resetPin);
				if (it != res.clock2ResetPinIdx.end()) {
					idx = it->second;
				} else {
					idx = res.resetPins.size();
					res.resetPins.push_back({});
					res.clock2ResetPinIdx[resetPin] = idx;
				}

				res.resetPins[idx].clocks.push_back(clock);
				res.clock2ResetPinIdx[clock] = idx;

				if (resetPin == clock) {
					res.resetPins[idx].source = clock;
					res.resetPins[idx].minResetCycles = clock->getMinResetCycles();
					res.resetPins[idx].minResetTime = clock->getMinResetTime();
				}
			}
		}
	}

	for (auto &c : res.clockPins) {
		HCL_ASSERT(c.source != nullptr);
		HCL_ASSERT(!c.clocks.empty());
	}
	for (auto &c : res.resetPins) {
		HCL_ASSERT(c.source != nullptr);
		HCL_ASSERT(!c.clocks.empty());
	}

	return res;
}



}