/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include <optional>
#include <variant>

#include "../../simulation/SigHandle.h"
#include "../../simulation/BitVectorState.h"
#include "MemoryStorage.h"

namespace gtry::hlim {

class Circuit;
class Clock;


/**
 * @brief Describes (in backend-terms) the memory and its interfaces.
 * 
 */
struct MemorySimConfig {
	size_t size;
	bool sparse = false;
	MemoryStorage::Initialization initialization;

	struct RdPrtNodePorts {
		Clock *clk = nullptr;
		sim::SigHandle addr;
		std::optional<sim::SigHandle> en;
		sim::SigHandle data;
		size_t width = 0;
		size_t inputLatency = 1;
		size_t outputLatency = 0;

		enum ReadDuringWrite {
			READ_BEFORE_WRITE,
			READ_AFTER_WRITE,
			READ_UNDEFINED
		};
		ReadDuringWrite rdw = READ_UNDEFINED;

		bool async() const { return inputLatency == 0 && outputLatency == 0; }
	};

	struct WrPrtNodePorts {
		Clock *clk = nullptr;
		sim::SigHandle addr;
		std::optional<sim::SigHandle> en;
		sim::SigHandle data;
		std::optional<sim::SigHandle> wrMask;
		size_t width = 0;
		size_t inputLatency = 1;
	};

	/// List of all read ports
	std::vector<RdPrtNodePorts> readPorts;
	/// List of all write ports
	std::vector<WrPrtNodePorts> writePorts;

	bool warnRWCollision = false;
	bool warnUncontrolledWrite = false;

	//std::function<sim::SimulationFunction<>(sim::DefaultBitVectorState &memoryState)> memoryStateAccessor;
};

void addExternalMemorySimulator(Circuit &circuit, MemorySimConfig config);

}
