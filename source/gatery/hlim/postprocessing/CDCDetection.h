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

#include <functional>
#include <vector>
#include <map>

namespace gtry::hlim {

class Circuit;
class Clock;

class Subnet;
class ConstSubnet;
class BaseNode;
struct NodePort;

struct SignalClockDomain {
	enum Type {
		UNKNOWN,
		CONSTANT,
		CLOCK
	};
	Type type = UNKNOWN;
	Clock *clk = nullptr;
};

void inferClockDomains(Circuit &circuit, std::map<hlim::NodePort, SignalClockDomain> &domains);

void detectUnguardedCDCCrossings(Circuit &circuit, const ConstSubnet &subnet, std::function<void(const BaseNode*, size_t)> detectionCallback);


}
