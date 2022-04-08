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

namespace gtry::hlim {


Node_Pin::Node_Pin(bool inputPin) : Node(inputPin?0:1, inputPin?1:0), m_isInputPin(inputPin)
{
	if (m_isInputPin)
		setOutputType(0, OUTPUT_IMMEDIATE);
}

void Node_Pin::connect(const NodePort &port)
{ 
	HCL_DESIGNCHECK(!m_isInputPin);
	NodeIO::connectInput(0, port);
	if (port.node != nullptr)
		m_connectionType = port.node->getOutputConnectionType(port.port);
}

void Node_Pin::setBool()
{
	HCL_DESIGNCHECK(m_isInputPin);
	m_connectionType = {.interpretation = ConnectionType::BOOL, .width = 1};
	setOutputConnectionType(0, m_connectionType);
}

void Node_Pin::setWidth(size_t width)
{
	HCL_DESIGNCHECK(m_isInputPin);
	m_connectionType = {.interpretation = ConnectionType::BITVEC, .width = width};
	setOutputConnectionType(0, m_connectionType);
}


std::vector<size_t> Node_Pin::getInternalStateSizes() const
{
	if (!m_isInputPin) return {};

	return {getOutputConnectionType(0).width};
}

void Node_Pin::setState(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const sim::DefaultBitVectorState &newState)
{
	HCL_ASSERT(m_isInputPin);
	HCL_ASSERT(newState.size() == getOutputConnectionType(0).width);
	state.copyRange(internalOffsets[0], newState, 0, newState.size());
}

void Node_Pin::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	if (m_isInputPin)
		state.copyRange(outputOffsets[0], state, internalOffsets[0], getOutputConnectionType(0).width);
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
	return "in";
}

std::string Node_Pin::getOutputName(size_t idx) const
{
	return "out";
}

std::unique_ptr<BaseNode> Node_Pin::cloneUnconnected() const {
	Node_Pin *other;
	std::unique_ptr<BaseNode> copy(other = new Node_Pin(m_isInputPin));
	copyBaseToClone(copy.get());
	other->m_connectionType = m_connectionType;
	other->m_differential = m_differential;
	other->m_differentialPosName = m_differentialPosName;
	other->m_differentialNegName = m_differentialNegName;

	return copy;
}

std::string Node_Pin::attemptInferOutputName(size_t outputPort) const
{
	return m_name;
}

void Node_Pin::setDifferential(std::string_view posPrefix, std::string_view negPrefix)
{
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
