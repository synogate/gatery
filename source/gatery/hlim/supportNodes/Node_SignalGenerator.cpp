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
#include "Node_SignalGenerator.h"

#include "../../utils/Range.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {

Node_SignalGenerator::Node_SignalGenerator(Clock *clk) {
	m_clocks.resize(1);
	attachClock(clk, 0);
}

void Node_SignalGenerator::setOutputs(const std::vector<ConnectionType> &connections)
{
	resizeOutputs(connections.size());
	for (auto i : utils::Range(connections.size())) {
		setOutputConnectionType(i, connections[i]);
		setOutputType(i, OUTPUT_LATCHED);
	}
}

void Node_SignalGenerator::resetDataDefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const
{
	for (auto i : utils::Range(getNumOutputPorts())) {
		state.setRange(sim::DefaultConfig::VALUE, outputOffsets[i], getOutputConnectionType(i).width, false);
		state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[i], getOutputConnectionType(i).width, true);
	}
}

void Node_SignalGenerator::resetDataUndefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const
{
	for (auto i : utils::Range(getNumOutputPorts())) {
		state.setRange(sim::DefaultConfig::VALUE, outputOffsets[i], getOutputConnectionType(i).width, false);
		state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[i], getOutputConnectionType(i).width, false);
	}
}

void Node_SignalGenerator::simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const
{
	std::uint64_t &tick = state.data(sim::DefaultConfig::VALUE)[internalOffsets[0]/64];
	tick = 0;
	produceSignals(state, outputOffsets, tick);
}

void Node_SignalGenerator::simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const
{
	std::uint64_t &tick = state.data(sim::DefaultConfig::VALUE)[internalOffsets[0]/64];
	tick++;
	produceSignals(state, outputOffsets, tick);
}

std::string Node_SignalGenerator::getTypeName() const 
{ 
	return "SignalGenerator";
}

void Node_SignalGenerator::assertValidity() const 
{ 

}

std::string Node_SignalGenerator::getInputName(size_t idx) const 
{
	return "";
}

std::vector<size_t> Node_SignalGenerator::getInternalStateSizes() const 
{
	return {64};
}

}
