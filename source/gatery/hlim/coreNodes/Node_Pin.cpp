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
#include "Node_Pin.h"

#include "../SignalDelay.h"


#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {


Node_Pin::Node_Pin(bool inputPin, bool outputPin, bool hasOutputEnable) : Node(2, 1), m_isInputPin(inputPin), m_isOutputPin(outputPin), m_hasOutputEnable(hasOutputEnable)
{
	m_clocks.resize(1);
	setOutputType(0, OUTPUT_IMMEDIATE);
}

void Node_Pin::connect(const NodePort &port)
{ 
	HCL_DESIGNCHECK(m_isOutputPin);
	NodeIO::connectInput(0, port);
	if (port.node != nullptr) {
		m_connectionType = port.node->getOutputConnectionType(port.port);

		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( m_connectionType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setOutputConnectionType(0, m_connectionType);
	}
}

void Node_Pin::setClockDomain(Clock *clk)
{
	attachClock(clk, 0);
}


void Node_Pin::connectEnable(const NodePort &port)
{ 
	HCL_DESIGNCHECK(m_isOutputPin);
	NodeIO::connectInput(1, port);
}

void Node_Pin::setBool()
{
	HCL_DESIGNCHECK(m_isInputPin);
	m_connectionType = {.type = ConnectionType::BOOL, .width = 1};
	setOutputConnectionType(0, m_connectionType);
}

void Node_Pin::setWidth(size_t width)
{
	HCL_DESIGNCHECK(m_isInputPin);
	m_connectionType = {.type = ConnectionType::BITVEC, .width = width};
	setOutputConnectionType(0, m_connectionType);
}


std::vector<size_t> Node_Pin::getInternalStateSizes() const
{
	if (!m_isInputPin) return {};

	return { getOutputConnectionType(0).width };
}

bool Node_Pin::setState(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const sim::ExtendedBitVectorState &newState)
{
	HCL_ASSERT(m_isInputPin);
	HCL_ASSERT(newState.size() == getOutputConnectionType(0).width);
#if 0
	bool equal = state.compareRange(internalOffsets[0], newState, 0, newState.size());
	if (!equal)
		state.copyRange(internalOffsets[0], newState, 0, newState.size());
	return !equal;
#else
	bool equal = true;
	for (size_t i = 0; i < newState.size(); i++) {
		bool newValue = false;
		bool newDefined = false;
		HCL_DESIGNCHECK_HINT(!newState.get(sim::ExtendedConfig::DONT_CARE, i), "Can not set DONT_CARE to a pin. You either meant to compare, or you wanted Z (high impedance).");
		if (newState.get(sim::ExtendedConfig::HIGH_IMPEDANCE, i)) {
			switch (m_param.highImpedanceValue) {
				case PinNodeParameter::HighImpedanceValue::UNDEFINED:
					newDefined = false;
				break;
				case PinNodeParameter::HighImpedanceValue::PULL_UP:
					newValue = true;
					newDefined = true;
				break;
				case PinNodeParameter::HighImpedanceValue::PULL_DOWN:
					newValue = false;
					newDefined = true;
				break;
			}
		} else {
			newValue = newState.get(sim::ExtendedConfig::VALUE, i);
			newDefined = newState.get(sim::ExtendedConfig::DEFINED, i);
		}
		bool oldValue = state.get(sim::DefaultConfig::VALUE, internalOffsets[0] + i);
		bool oldDefined = state.get(sim::DefaultConfig::DEFINED, internalOffsets[0] + i);

		if (oldDefined != newDefined) 
			equal = false;

		if ((oldValue != newValue) && oldDefined)
			equal = false;

		state.set(sim::DefaultConfig::VALUE, internalOffsets[0] + i, newValue);
		state.set(sim::DefaultConfig::DEFINED, internalOffsets[0] + i, newDefined);
	}
	return !equal;
#endif
}

void Node_Pin::simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
	if (!m_isInputPin) return;

	size_t size = getOutputConnectionType(0).width;
	switch (m_param.highImpedanceValue) {
		case PinNodeParameter::HighImpedanceValue::UNDEFINED:
			state.clearRange(sim::DefaultConfig::DEFINED, internalOffsets[0], size);
		break;
		case PinNodeParameter::HighImpedanceValue::PULL_UP:
			state.setRange(sim::DefaultConfig::DEFINED, internalOffsets[0], size);
			state.setRange(sim::DefaultConfig::VALUE, internalOffsets[0], size);
		break;
		case PinNodeParameter::HighImpedanceValue::PULL_DOWN:
			state.setRange(sim::DefaultConfig::DEFINED, internalOffsets[0], size);
			state.clearRange(sim::DefaultConfig::VALUE, internalOffsets[0], size);
		break;
	}
}

void Node_Pin::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (m_isInputPin) {
		if (m_isOutputPin) {
			bool outputEnabled = true;
			bool outputEnabledDefined = true;

			if (m_hasOutputEnable && inputOffsets[1] != ~0ull) {
				outputEnabled = state.get(sim::DefaultConfig::VALUE, inputOffsets[1]);
				outputEnabledDefined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[1]);
			}

			if (!outputEnabledDefined) {
				state.clearRange(sim::DefaultConfig::DEFINED,  outputOffsets[0], getOutputConnectionType(0).width);
			} else {
				if (outputEnabled) {
					// what is being driven overrides what the simulation process set

					if (inputOffsets[0] == ~0ull)
						state.clearRange(sim::DefaultConfig::DEFINED,  outputOffsets[0], getOutputConnectionType(0).width);
					else
						state.copyRange(outputOffsets[0], state, inputOffsets[0], getOutputConnectionType(0).width);
				} else {
					// copy whatever was set via simulation processes
					state.copyRange(outputOffsets[0], state, internalOffsets[0], getOutputConnectionType(0).width);
				}
			}
		} else // copy whatever was set via simulation processes
			state.copyRange(outputOffsets[0], state, internalOffsets[0], getOutputConnectionType(0).width);
	}
}

std::string Node_Pin::getTypeName() const
{
	if (m_differential)
		return "ioPin_differential";
	else
		return "ioPin";
}

void Node_Pin::assertValidity() const
{

}

std::string Node_Pin::getInputName(size_t idx) const
{
	switch (idx) {
		case 0: return "in";
		case 1: return "outputEnable";
		default: return "";
	}
}

std::string Node_Pin::getOutputName(size_t idx) const
{
	return "out";
}

std::unique_ptr<BaseNode> Node_Pin::cloneUnconnected() const {
	Node_Pin *other;
	std::unique_ptr<BaseNode> copy(other = new Node_Pin(m_isInputPin, m_isOutputPin, m_hasOutputEnable));
	copyBaseToClone(copy.get());
	other->m_connectionType = m_connectionType;
	other->m_differential = m_differential;
	other->m_differentialPosName = m_differentialPosName;
	other->m_differentialNegName = m_differentialNegName;
	other->m_param = m_param;

	return copy;
}

std::string Node_Pin::attemptInferOutputName(size_t outputPort) const
{
	return m_name;
}

void Node_Pin::setDifferential(std::string_view posPrefix, std::string_view negPrefix)
{
	HCL_DESIGNCHECK_HINT(!m_hasOutputEnable, "Not implemented!");
	m_differential = true;
	m_differentialPosName = m_name + std::string(posPrefix);
	m_differentialNegName = m_name + std::string(negPrefix);
}



void Node_Pin::estimateSignalDelay(SignalDelay &sigDelay)
{
	for (auto i : utils::Range(getNumOutputPorts())) {
		HCL_ASSERT(sigDelay.contains({.node = this, .port = i}));
		auto outDelay = sigDelay.getDelay({.node = this, .port = i});
		for (auto &f : outDelay)
			f = 0.0f;
	}
}

void Node_Pin::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


}
