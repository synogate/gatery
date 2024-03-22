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
#include "Node_Register.h"
#include "Node_Constant.h"
#include "../Clock.h"

#include "../SignalDelay.h"
#include "../GraphTools.h"
#include "../NodeGroup.h"

#include <regex>

namespace gtry::hlim {

Node_Register::Node_Register() : Node(NUM_INPUTS, 1)
{
	m_clocks.resize(1);
	setOutputType(0, OUTPUT_LATCHED);
}

void Node_Register::connectInput(Input input, const NodePort &port)
{
	NodeIO::connectInput(input, port);
	if (port.node != nullptr)
		if (input == DATA || input == RESET_VALUE)
			setOutputConnectionType(0, port.node->getOutputConnectionType(port.port));
}

void Node_Register::setClock(Clock *clk)
{
	attachClock(clk, 0);
}

void Node_Register::simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
	//if (m_clocks[0]->getRegAttribs().initializeRegs) {
		writeResetValueTo(state, { internalOffsets[INT_DATA],  outputOffsets[0] }, getOutputConnectionType(0).width, true);
		state.set(sim::DefaultConfig::VALUE, internalOffsets[INT_IN_RESET], false);
	//}
}

void Node_Register::simulateResetChange(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort, bool resetHigh) const
{
	bool inReset = (resetHigh ^ !(m_clocks[0]->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH)) && (getNonSignalDriver(RESET_VALUE).node != nullptr);
	state.set(sim::DefaultConfig::VALUE, internalOffsets[INT_IN_RESET], inReset);

	if (inReset && m_clocks[0]->getRegAttribs().resetType == RegisterAttributes::ResetType::ASYNCHRONOUS) {
		writeResetValueTo(state, { internalOffsets[INT_DATA],  outputOffsets[0] }, getOutputConnectionType(0).width, false);
	}
}

void Node_Register::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (inputOffsets[DATA] == ~0ull)
		state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[INT_DATA], getOutputConnectionType(0).width);
	else
		state.copyRange(internalOffsets[INT_DATA], state, inputOffsets[DATA], getOutputConnectionType(0).width);

	if (inputOffsets[ENABLE] == ~0ull) {
		state.set(sim::DefaultConfig::DEFINED, internalOffsets[INT_ENABLE], 1);
		state.set(sim::DefaultConfig::VALUE, internalOffsets[INT_ENABLE], 1);
	} else
		state.copyRange(internalOffsets[INT_ENABLE], state, inputOffsets[ENABLE], 1);
}

void Node_Register::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
	HCL_ASSERT(clockPort == 0);

	if (state.get(sim::DefaultConfig::VALUE, internalOffsets[INT_IN_RESET])) {
		if (m_clocks[0]->getRegAttribs().resetType == RegisterAttributes::ResetType::SYNCHRONOUS) {
			writeResetValueTo(state, { internalOffsets[INT_DATA],  outputOffsets[0] }, getOutputConnectionType(0).width, false);
		} else {
			// Is being handled in Node_Register::simulateResetChange
		}
	} else {
		bool enableDefined = state.get(sim::DefaultConfig::DEFINED, internalOffsets[INT_ENABLE]);
		bool enable = state.get(sim::DefaultConfig::VALUE, internalOffsets[INT_ENABLE]);

		if (!enableDefined) {
			state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width);
		} else
			if (enable)
				state.copyRange(outputOffsets[0], state, internalOffsets[INT_DATA], getOutputConnectionType(0).width);
	}
}

bool Node_Register::overrideOutput(sim::DefaultBitVectorState &state, size_t outputOffset, const sim::DefaultBitVectorState &newState)
{
	HCL_ASSERT(newState.size() == getOutputConnectionType(0).width);
	bool equal = state.compareRange(outputOffset, newState, 0, newState.size());
	if (!equal)
		state.copyRange(outputOffset, newState, 0, newState.size());
	return !equal;
}


std::string Node_Register::getTypeName() const
{
	return "Register";
}

void Node_Register::assertValidity() const
{

}

std::string Node_Register::getInputName(size_t idx) const
{
	switch (idx) {
		case DATA: return "data_in";
		case RESET_VALUE: return "reset_value";
		case ENABLE: return "enable";
		default:
			return "INVALID";
	}
}

std::string Node_Register::getOutputName(size_t idx) const
{
	return "data_out";
}

std::vector<size_t> Node_Register::getInternalStateSizes() const
{
	std::vector<size_t> res(NUM_INTERNALS);
	res[INT_DATA] = getOutputConnectionType(0).width;
	res[INT_ENABLE] = 1;
	res[INT_IN_RESET] = 1;
	return res;
}



std::unique_ptr<BaseNode> Node_Register::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Register());
	copyBaseToClone(res.get());
	((Node_Register*)res.get())->m_flags = m_flags;
	return res;
}

std::string Node_Register::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;

	auto driver0 = getDriver(0);
	if (driver0.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(0)) return "";
	if (driver0.node->getName().empty()) return "";

	std::regex expression("(.*)_delayed(\\d+)");
	std::smatch matches;
	if (std::regex_match(driver0.node->getName(), matches, expression)) {
		name << matches[1].str() << "_delayed" << (boost::lexical_cast<size_t>(matches[2].str())+1);
	} else
		name << driver0.node->getName() << "_delayed1";
	return name.str();
}


void Node_Register::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});
	for (auto &f : outDelay)
		f = 0.0f;
}

void Node_Register::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


void Node_Register::writeResetValueTo(sim::DefaultBitVectorState &state, const std::array<size_t, 2> &offsets, size_t width, bool clearDefinedIfUnconnected) const
{
	auto resetDriver = getNonSignalDriver(RESET_VALUE);
	if (resetDriver.node == nullptr) {
		if (clearDefinedIfUnconnected)
			for (auto offset : offsets)
				state.clearRange(sim::DefaultConfig::DEFINED, offset, width);
		return;
	}
	Node_Constant *constNode = dynamic_cast<Node_Constant *>(resetDriver.node);

	if (constNode != nullptr) {
		HCL_ASSERT(constNode->getValue().size() == width);
		for (auto offset : offsets)
			state.insert(constNode->getValue(), offset);

		return;
	}

	auto evalState = evaluateStatically(m_nodeGroup->getCircuit(), resetDriver);
	HCL_ASSERT(evalState.size() == width);
	HCL_ASSERT_HINT(sim::allDefined(evalState), "Can not determine reset value of register!");	
	for (auto offset : offsets)
		state.insert(evalState, offset);
}

bool Node_Register::inputIsEnable(size_t inputPort) const
{
	return inputPort == ENABLE;
}

}
