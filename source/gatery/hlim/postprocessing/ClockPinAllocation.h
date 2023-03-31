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
#pragma once

#include <vector>
#include <map>

#include <gatery/utils/StableContainers.h>

#include "../ClockRational.h"

namespace gtry::hlim {

class Circuit;
class Clock;
class Subnet;

struct ClockPinAllocation {
	
	struct ClockPin {
		Clock *source = nullptr;
		std::vector<Clock*> clocks;
	};

	struct ResetPin {
		Clock *source = nullptr;
		std::vector<Clock*> clocks;

		ClockRational minResetTime = {};
		size_t minResetCycles = 0;
	};

	std::vector<ClockPin> clockPins;
	std::vector<ResetPin> resetPins;

	utils::StableMap<Clock*, size_t> clock2ClockPinIdx;
	utils::StableMap<Clock*, size_t> clock2ResetPinIdx;
};

/**
 * @brief Extracts (deduplicates) all the individual clock and reset signals from a circuit.
 * @param circuit Circuit from which to extract the pins
 * @param subnet Limits the considered clock to clocks that drive at least one node in this subnet.
 * @return ClockPinAllocation Deduplicated clock and reset signals with Clock* -> pin and pin -> Clock* mappings
 */
ClockPinAllocation extractClockPins(Circuit &circuit, const Subnet &subnet);


}
