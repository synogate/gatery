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
#include "Node_Constant.h"

#include "../SignalDelay.h"


#include <sstream>
#include <iomanip>

namespace gtry::hlim {

Node_Constant::Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType connectionType) :
	Node(0, 1),
	m_Value(std::move(value))
{
	HCL_ASSERT(connectionType.width == m_Value.size());
	setOutputConnectionType(0, connectionType);
	setOutputType(0, OUTPUT_CONSTANT);
}


Node_Constant::Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType::Type connectionType) :
	Node_Constant(std::move(value), {.type = connectionType, .width = value.size() })
{
}

Node_Constant::Node_Constant(bool value) : Node_Constant(sim::parseBit(value), {.type = hlim::ConnectionType::BOOL, .width = 1 })
{
}

void Node_Constant::simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
	state.insert(m_Value, outputOffsets[0]);
}

std::string Node_Constant::getTypeName() const
{
	std::stringstream bitStream;
	bitStream << std::hex << m_Value;
	return bitStream.str();
}

void Node_Constant::assertValidity() const
{

}

std::string Node_Constant::getInputName(size_t idx) const
{
	return "";
}

std::string Node_Constant::getOutputName(size_t idx) const
{
	return "output";
}


std::unique_ptr<BaseNode> Node_Constant::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Constant(m_Value, getOutputConnectionType(0).type));
	copyBaseToClone(res.get());
	return res;
}

std::string Node_Constant::attemptInferOutputName(size_t outputPort) const
{
	if (!m_name.empty()) return m_name;

	if (m_Value.size() == 0)
		return "const_empty";
	
	std::stringstream bitStream;
	bitStream << "const_";
	sim::formatState(bitStream, m_Value, 16, true);
	return bitStream.str();
}

void Node_Constant::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});
	for (auto &f : outDelay) f = 0.0f;
}

void Node_Constant::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}


}
