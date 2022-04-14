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

#include "../BitVectorState.h"

#include "../../hlim/NodePort.h"
#include "../../hlim/ClockRational.h"

#include <map>
#include <string>
#include <vector>

namespace gtry::hlim {

class Clock;

}

namespace gtry::sim {

struct MemoryTrace {
	struct Signal {
		hlim::RefCtdNodePort driver;
		const hlim::Clock *clock = nullptr;
		std::string name;
		size_t width;
		bool isBool;
	};

	struct SignalChange {
		size_t sigIdx;
		size_t dataOffset;
	};

	struct Event {
		hlim::ClockRational timestamp;
		std::vector<SignalChange> changes;
	};

	DefaultBitVectorState data;
	std::vector<Signal> signals;
	std::vector<Event> events;

	struct Annotation {
		struct Range {
			std::string desc;
			hlim::ClockRational start;
			hlim::ClockRational end;
		};
		std::vector<Range> ranges;
	};

	std::map<std::string, Annotation> annotations;

	inline void clear() {
		data.clear();
		signals.clear();
		events.clear();
		annotations.clear();
	}
};

}
