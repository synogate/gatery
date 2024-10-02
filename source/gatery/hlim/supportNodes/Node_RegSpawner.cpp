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

#include "Node_RegSpawner.h"

#include "../NodeGroup.h"

#include "../SignalDelay.h"

#include "../coreNodes/Node_Register.h"
#include "../Circuit.h"

#include <gatery/simulation/BitVectorState.h>

namespace gtry::hlim {


Node_RegSpawner::Node_RegSpawner() : Node(INPUT_SIGNAL_OFFSET, 0)
{
	m_clocks.resize(1);
}

void Node_RegSpawner::setClock(Clock *clk)
{
	attachClock(clk, 0);
}

void Node_RegSpawner::markResolved()
{
	m_wasResolved = true;
	for (auto i : utils::Range(getNumOutputPorts()))
		bypassOutputToInput(i, i*2+INPUT_SIGNAL_OFFSET);
}

std::vector<Node_Register*> Node_RegSpawner::spawnForward()
{
	HCL_DESIGNCHECK_HINT(!m_wasResolved, "Trying to use a register spawner for register retiming that was already resolved in an earlier retiming run."
		"This is not allowed as other design choices might have been made on the number of spawned registers (pipeline stages) that the spawner comitted to before!");
	auto &circuit = m_nodeGroup->getCircuit();

	std::vector<Node_Register*> result;
	result.reserve(getNumOutputPorts());

	NodePort enable = getDriver(0);

	// For each signal passing through
	for (auto i : utils::Range(getNumOutputPorts())) {
		// create register
		auto *reg = circuit.createNode<Node_Register>();
		reg->moveToGroup(m_nodeGroup);
		reg->recordStackTrace();

		// allow further retiming as this is their primary purpose
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_BACKWARD);
		reg->getFlags().insert(Node_Register::Flags::ALLOW_RETIMING_FORWARD);

		// With our clock
		reg->setClock(m_clocks[0]);

		// Set reset value
		reg->connectInput(Node_Register::RESET_VALUE, getDriver(i*2+1+INPUT_SIGNAL_OFFSET));

		// Set enable
		if (enable.node != nullptr)
			reg->connectInput(Node_Register::ENABLE, enable);

		// Fetch everything driven by the signal passing through
		auto driven = getDirectlyDriven(i);

		// Drive register with signal passing through (thus putting the register behind the spawner node).
		reg->connectInput(Node_Register::DATA, {.node = this, .port = i});

		// Reassign everything driven by reg spawner to register
		for (auto &np : driven)
			np.node->rewireInput(np.port, {.node = reg, .port = 0ull});

		result.push_back(reg);
	}

	m_numStagesSpawned++;
	return result;
}

void Node_RegSpawner::setEnableCondition(const NodePort &value)
{
	NodeIO::connectInput(INPUT_ENABLE, value);
}

NodePort Node_RegSpawner::getEnableCondition()
{
	return getDriver(INPUT_ENABLE);
}

size_t Node_RegSpawner::addInput(const NodePort &value, const NodePort &reset)
{
	size_t port = getNumOutputPorts();
	resizeInputs((port+1)*2+INPUT_SIGNAL_OFFSET);
	resizeOutputs(port+1);

	auto type = hlim::getOutputConnectionType(value);
	setOutputConnectionType(port, type);

	NodeIO::connectInput(port*2+0+INPUT_SIGNAL_OFFSET, value);
	NodeIO::connectInput(port*2+1+INPUT_SIGNAL_OFFSET, reset);

	return port;
}

void Node_RegSpawner::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, 
					const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
	for (auto i : utils::Range(getNumOutputPorts())) {
		if (inputOffsets[i*2+0+INPUT_SIGNAL_OFFSET] != ~0ull)
			state.copyRange(outputOffsets[i], state, inputOffsets[i*2+0+INPUT_SIGNAL_OFFSET], getOutputConnectionType(i).width);
		else
			state.clearRange(sim::DefaultConfig::DEFINED, outputOffsets[i], getOutputConnectionType(i).width);
	}
}

std::string Node_RegSpawner::getTypeName() const
{
	return "reg_spawner";
}

void Node_RegSpawner::assertValidity() const
{
}

std::string Node_RegSpawner::getInputName(size_t idx) const
{
	if (idx % 2 == 0)
		return (boost::format("in_value_%d") % idx).str();
	return (boost::format("in_reset_%d") % idx).str();
}

std::string Node_RegSpawner::getOutputName(size_t idx) const
{
	return (boost::format("out_%d") % idx).str();
}

std::vector<size_t> Node_RegSpawner::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_RegSpawner::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_RegSpawner());
	copyBaseToClone(copy.get());

	return copy;
}


void Node_RegSpawner::estimateSignalDelay(SignalDelay &sigDelay)
{
	HCL_ASSERT(sigDelay.contains({.node = this, .port = 0ull}));
	auto outDelay = sigDelay.getDelay({.node = this, .port = 0ull});

	for (auto &f : outDelay)
		f = 0.0f;
}

void Node_RegSpawner::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = ~0u;
	inputBit = ~0u;
}

bool Node_RegSpawner::inputIsEnable(size_t inputPort) const
{
	// This is technically not true, but since the register spawner is allowed to spawn registers and thus change the graph's behavior
	// wrt. retiming it behaves in the same way.
	return inputPort == INPUT_ENABLE;
}


}