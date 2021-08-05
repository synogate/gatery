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

#include "../simulation/BitVectorState.h"

namespace gtry::hlim {

	class BaseNode;
	struct NodePort;
	class Circuit;
	class NodeGroup;
	class Node_Pin;
	class Clock;
	class Node_Register;

	sim::DefaultBitVectorState evaluateStatically(Circuit &circuit, hlim::NodePort output);

	Node_Pin* findInputPin(NodePort output);
	Node_Pin* findOutputPin(NodePort output);

	Clock* findFirstOutputClock(NodePort output);
	Clock* findFirstInputClock(NodePort input);

	std::vector<Node_Register*> findAllOutputRegisters(NodePort output);
	std::vector<Node_Register*> findAllInputRegisters(NodePort input);


}
