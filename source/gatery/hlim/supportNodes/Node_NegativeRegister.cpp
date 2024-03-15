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

#include "Node_NegativeRegister.h"

#include "../SignalDelay.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {

Node_NegativeRegister::Node_NegativeRegister() : Node((size_t) Inputs::count, (size_t) Outputs::count)
{
	setOutputConnectionType((size_t) Outputs::enable, { .type = ConnectionType::BOOL, .width = 1 });
}


void Node_NegativeRegister::setConnectionType(const ConnectionType &connectionType)
{
	setOutputConnectionType((size_t) Outputs::data, connectionType);
}

void Node_NegativeRegister::input(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = hlim::getOutputConnectionType(nodePort);
		auto myType = getOutputConnectionType((size_t) Outputs::data);
		if (!getDirectlyDriven((size_t) Outputs::data).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setConnectionType(paramType);
	}

	NodeIO::connectInput((size_t)Inputs::data, nodePort);
}

void Node_NegativeRegister::expectedEnable(const NodePort &nodePort)
{
	NodeIO::connectInput((size_t)Inputs::expectedEnable, nodePort);
}

void Node_NegativeRegister::disconnectInput()
{
	NodeIO::disconnectInput(0);
}


void Node_NegativeRegister::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, 
					const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	/// @todo: Is this dangerous?
	if (inputOffsets[0] != ~0ull)
		state.copyRange(outputOffsets[0], state, inputOffsets[0], getOutputConnectionType(0).width);
	else
		state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}

std::string Node_NegativeRegister::getTypeName() const
{
	return "neg_reg";
}

void Node_NegativeRegister::assertValidity() const
{
}

std::string Node_NegativeRegister::getInputName(size_t idx) const
{
	switch (idx) {
		case (size_t) Inputs::data: return "in";
		case (size_t) Inputs::expectedEnable: return "expectedEnable";
		default: return "invalid";
	}
}

std::string Node_NegativeRegister::getOutputName(size_t idx) const
{
	switch (idx) {
		case (size_t) Outputs::data: return "data";
		case (size_t) Outputs::enable: return "enable";
		default: return "invalid";
	}
}


std::vector<size_t> Node_NegativeRegister::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_NegativeRegister::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_NegativeRegister());
	copyBaseToClone(copy.get());

	return copy;
}


void Node_NegativeRegister::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	for (auto &f : outDelay)
		f = 0.0f;
}

void Node_NegativeRegister::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


}