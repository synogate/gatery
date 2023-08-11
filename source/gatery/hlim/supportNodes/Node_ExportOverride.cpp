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

#include "Node_ExportOverride.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {


Node_ExportOverride::Node_ExportOverride() : Node(2, 1)
{

}


void Node_ExportOverride::setConnectionType(const ConnectionType &connectionType)
{
	setOutputConnectionType(0, connectionType);
}

void Node_ExportOverride::connectInput(const NodePort &nodePort)
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
					"The signal and override value connection types must be the same.");

	NodeIO::connectInput(SIM_INPUT, nodePort);
}

void Node_ExportOverride::connectOverride(const NodePort &nodePort)
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
					"The signal and override value connection types must be the same.");

	NodeIO::connectInput(EXP_INPUT, nodePort);
}

void Node_ExportOverride::simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const
{
	std::cout << "Warning: Node_ExportOverride::simulateEvaluate called!" << std::endl;
	if (inputOffsets[SIM_INPUT] != ~0ull)
		state.copyRange(outputOffsets[0], state, inputOffsets[SIM_INPUT], getOutputConnectionType(0).width);
	else
		state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
}


void Node_ExportOverride::disconnectInput()
{
	NodeIO::disconnectInput(0);
}


std::string Node_ExportOverride::getTypeName() const
{
	return "export_override";
}

void Node_ExportOverride::assertValidity() const
{
}

std::string Node_ExportOverride::getInputName(size_t idx) const
{
	switch (idx) {
		case 0: return "sim_in";
		case 1: return "export_in";
		default: return "invalid";
	}
}

std::string Node_ExportOverride::getOutputName(size_t idx) const
{
	return "out";
}


std::vector<size_t> Node_ExportOverride::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_ExportOverride::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_ExportOverride());
	copyBaseToClone(copy.get());

	return copy;
}



std::string Node_ExportOverride::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;


	auto driver0 = getDriver(SIM_INPUT);
	if (driver0.node != nullptr && inputIsComingThroughParentNodeGroup(SIM_INPUT) && !driver0.node->getName().empty()) return driver0.node->getName() + "_export_override";

	auto driver1 = getDriver(EXP_INPUT);
	if (driver1.node != nullptr && inputIsComingThroughParentNodeGroup(EXP_INPUT) && !driver1.node->getName().empty()) return driver1.node->getName() + "_export_override";

	return "";
}



}
