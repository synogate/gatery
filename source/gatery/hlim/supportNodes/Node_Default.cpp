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

#include "Node_Default.h"

#include "../SignalDelay.h"

namespace gtry::hlim {


Node_Default::Node_Default() : Node(2, 1)
{

}


void Node_Default::setConnectionType(const ConnectionType &connectionType)
{
	setOutputConnectionType(0, connectionType);
}

void Node_Default::connectInput(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = hlim::getOutputConnectionType(nodePort);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setConnectionType(paramType);
	}

	if (getDriver(0).node != nullptr && getDriver(1).node != nullptr)
		HCL_ASSERT_HINT(hlim::getOutputConnectionType(getDriver(0)) == hlim::getOutputConnectionType(getDriver(1)), 
					"The signal and default value connection types must be the same.");

	NodeIO::connectInput(0, nodePort);
}

void Node_Default::connectDefault(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = hlim::getOutputConnectionType(nodePort);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setConnectionType(paramType);
	}

	if (getDriver(0).node != nullptr && getDriver(1).node != nullptr)
		HCL_ASSERT_HINT(hlim::getOutputConnectionType(getDriver(0)) == hlim::getOutputConnectionType(getDriver(1)), 
					"The signal and default value connection types must be the same.");

	NodeIO::connectInput(1, nodePort);
}


void Node_Default::disconnectInput()
{
	NodeIO::disconnectInput(0);
}



void Node_Default::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, 
					const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	HCL_ASSERT_HINT(false, "Can not simulate Node_Default, must resolve first!");
}

std::string Node_Default::getTypeName() const
{
	return "default";
}

void Node_Default::assertValidity() const
{
}

std::string Node_Default::getInputName(size_t idx) const
{
	switch (idx) {
		case 0: return "in";
		case 1: return "default_value";
		default: return "invalid";
	}
}

std::string Node_Default::getOutputName(size_t idx) const
{
	return "out";
}


std::vector<size_t> Node_Default::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_Default::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_Default());
	copyBaseToClone(copy.get());

	return copy;
}


void Node_Default::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	for (auto &f : outDelay)
		f = 0.0f;
}

void Node_Default::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


}