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

#include "gatery/pch.h"

#include "Node_CDC.h"
#include "../Clock.h"

#include <gatery/simulation/BitVectorState.h>

#include <external/magic_enum.hpp>

namespace gtry::hlim {


Node_CDC::Node_CDC() : Node(1, 1)
{
	m_clocks.resize(magic_enum::enum_count<Clocks>());
	setOutputType(0, OUTPUT_IMMEDIATE);
}

void Node_CDC::connectInput(const NodePort &nodePort)
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


void Node_CDC::disconnectInput()
{
	NodeIO::disconnectInput(0);
}


void Node_CDC::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, 
					const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (inputOffsets[0] != ~0ull)
		state.copyRange(outputOffsets[0], state, inputOffsets[0], getOutputConnectionType(0).width);
	else
		state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}



std::string Node_CDC::getTypeName() const
{
	return "cdc";
}

void Node_CDC::assertValidity() const
{
}

std::string Node_CDC::getInputName(size_t idx) const
{
	return "in";
}

std::string Node_CDC::getOutputName(size_t idx) const
{
	return "out";
}


std::vector<size_t> Node_CDC::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_CDC::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_CDC());
	copyBaseToClone(copy.get());

	return copy;
}

OutputClockRelation Node_CDC::getOutputClockRelation(size_t output) const
{
	OutputClockRelation res;
	res.dependentClocks.push_back(m_clocks[(size_t)Clocks::OUTPUT_CLOCK]);
	return res;
}

bool Node_CDC::checkValidInputClocks(std::span<SignalClockDomain> inputClocks) const
{
	switch (inputClocks[0].type) {
		case SignalClockDomain::CONSTANT:
			return true;
		break;
		case SignalClockDomain::UNKNOWN:
			return false;
		break;
		case SignalClockDomain::CLOCK:
			HCL_ASSERT_HINT(m_clocks[(size_t)Clocks::INPUT_CLOCK] != nullptr, "Node_CDC must have it's clock ports bound to clocks!");
			return inputClocks[0].clk->getClockPinSource() == m_clocks[(size_t)Clocks::INPUT_CLOCK]->getClockPinSource();
		break;
	}
	return false;
}


}