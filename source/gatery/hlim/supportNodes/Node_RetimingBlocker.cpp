/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

#include "Node_RetimingBlocker.h"

#include "../NodeGroup.h"

#include "../SignalDelay.h"

#include "../coreNodes/Node_Register.h"
#include "../Circuit.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {


Node_RetimingBlocker::Node_RetimingBlocker() : Node(1, 1)
{

}

void Node_RetimingBlocker::connectInput(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = nodePort.node->getOutputConnectionType(nodePort.port);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setOutputConnectionType(0, paramType);
	}
	NodeIO::connectInput(0, nodePort);
}

void Node_RetimingBlocker::disconnectInput()
{
	NodeIO::disconnectInput(0);
}

void Node_RetimingBlocker::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, 
					const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (inputOffsets[0] != ~0ull)
		state.copyRange(outputOffsets[0], state, inputOffsets[0], getOutputConnectionType(0).width);
	else
		state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}

std::string Node_RetimingBlocker::getTypeName() const
{
	return "retiming_blocker";
}

void Node_RetimingBlocker::assertValidity() const
{
}

std::string Node_RetimingBlocker::getInputName(size_t idx) const
{
	return "in";
}

std::string Node_RetimingBlocker::getOutputName(size_t idx) const
{
	return "out";
}

std::vector<size_t> Node_RetimingBlocker::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_RetimingBlocker::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_RetimingBlocker());
	copyBaseToClone(copy.get());

	return copy;
}


void Node_RetimingBlocker::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	for (auto &f : outDelay)
		f = 0.0f;
}

void Node_RetimingBlocker::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


}