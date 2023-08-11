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
#include "WaveformRecorder.h"

#include "BitAllocator.h"
#include "Simulator.h"

#include "../hlim/Circuit.h"
#include "../hlim/supportNodes/Node_SignalTap.h"
#include "../hlim/supportNodes/Node_Memory.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <boost/format.hpp>

namespace gtry::sim {

WaveformRecorder::WaveformRecorder(hlim::Circuit &circuit, Simulator &simulator) : m_circuit(circuit), m_simulator(simulator)
{
	m_simulator.addCallbacks(this);
}

void WaveformRecorder::addSignal(hlim::NodePort driver, hlim::BaseNode *relevantNode, bool hidden)
{
	HCL_ASSERT(!hlim::outputIsDependency(driver));

	SignalReference sig = {
		.driver = driver,
		.relevantNode = hlim::NodePtr<hlim::BaseNode>(relevantNode)
	};

	auto it = m_alreadyAddedNodePorts.find(sig);
	if (it != m_alreadyAddedNodePorts.end()) {
		auto &signal = m_id2Signal[it->second];
		signal.isHidden &= hidden;
	} else {
		m_alreadyAddedNodePorts[sig] = m_id2Signal.size();

		Signal signal;
		signal.sortOrder = relevantNode->getId();
		signal.signalRef = sig;
		if (relevantNode->hasGivenName())
			signal.name = relevantNode->getName();
		else {
			std::string baseName = relevantNode->getName();
			if (baseName.empty())
				baseName = "unnamed";
			signal.name = (boost::format("%s_id_%d") % baseName % relevantNode->getId()).str();
		}
		signal.nodeGroup = relevantNode->getGroup();
		signal.isHidden = hidden;
		signal.isBVec = hlim::outputIsBVec(driver);
		signal.isPin = dynamic_cast<hlim::Node_Pin*>(relevantNode) != nullptr;
		signal.isTap = dynamic_cast<hlim::Node_SignalTap*>(relevantNode) != nullptr;
		m_id2Signal.push_back(signal);
	}
}

void WaveformRecorder::addMemory(hlim::Node_Memory *mem, hlim::NodeGroup *group, const std::string &nameOverride, size_t sortOrder)
{
	if (mem->getPorts().empty())
		return; // Ignore memories without any ports
	auto it = m_alreadyAddedMemories.find(mem);
	if (it == m_alreadyAddedMemories.end()) {
		m_alreadyAddedMemories[mem] = m_id2Signal.size();

		for (auto wordIdx : utils::Range(mem->getMaxDepth())) {
			Signal signal;
			signal.sortOrder = sortOrder;
			signal.memory = mem;
			signal.name = (boost::format("addr_%04d") % wordIdx).str();
			signal.nodeGroup = group;
			signal.isHidden = false;
			signal.isBVec = false;
			signal.isPin = false;
			signal.isTap = false;
			signal.memoryWordSize = mem->getMinPortWidth();
			signal.memoryWordIdx = wordIdx;
			m_id2Signal.push_back(signal);
		}
	}
}


void WaveformRecorder::addAllTaps()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *tap = dynamic_cast<hlim::Node_SignalTap*>(node.get()))
			if (tap->getLevel() == hlim::Node_SignalTap::LVL_WATCH)
				addSignal(tap->getDriver(0), tap, false);
}

void WaveformRecorder::addAllPins()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *pin = dynamic_cast<hlim::Node_Pin*>(node.get())) {
			if (pin->getConnectionType().width == 0) continue;
			if (pin->isOutputPin() && !pin->isInputPin()) {
				auto driver = pin->getDriver(0);
				if (driver.node != nullptr)
					addSignal(driver, pin, false);
			}
			if (pin->isInputPin())
				addSignal({.node = pin, .port = 0}, pin, false);
		}
}

void WaveformRecorder::addAllOutPins()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *pin = dynamic_cast<hlim::Node_Pin*>(node.get())) {
			if (!pin->isOutputPin()) continue;
			auto driver = pin->getDriver(0);
			if (driver.node != nullptr)
				addSignal(driver, pin, false);
		}
}

void WaveformRecorder::addAllNamedSignals()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *sig = dynamic_cast<hlim::Node_Signal*>(node.get())) {
			if (sig->hasGivenName())
				addSignal({.node=sig, .port=0ull}, sig, false);
		}
}

void WaveformRecorder::addAllSignals()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *sig = dynamic_cast<hlim::Node_Signal*>(node.get())) {
			addSignal({.node=sig, .port=0ull}, sig, !sig->hasGivenName());
		}
}

void WaveformRecorder::addAllMemories()
{
	for (auto &node : m_circuit.getNodes())
		if (auto *mem = dynamic_cast<hlim::Node_Memory*>(node.get())) {
			addMemory(mem, mem->getGroup(), {}, mem->getId());
		}
}

void WaveformRecorder::onAfterPowerOn()
{
	initializeStates();
	initialize();
	m_initialized = true;
}

void WaveformRecorder::initializeStates()
{
	BitAllocator allocator;

	m_id2StateOffsetSize.resize(m_id2Signal.size());
	for (auto id : utils::Range(m_id2Signal.size())) {
		auto &signal = m_id2Signal[id];
		size_t size;
		if (signal.signalRef.driver.node != nullptr)
			size = hlim::getOutputWidth(signal.signalRef.driver);
		else
			size = signal.memoryWordSize;
		auto offset = allocator.allocate((unsigned int)size);
		m_id2StateOffsetSize[id].offset = offset;
		m_id2StateOffsetSize[id].size = size;
	}
	m_trackedState.resize(allocator.getTotalSize());
	m_trackedState.clearRange(DefaultConfig::DEFINED, 0, allocator.getTotalSize());
}


void WaveformRecorder::onCommitState()
{
	for (auto id : utils::Range(m_id2Signal.size())) {
		auto &signal = m_id2Signal[id];
		auto offset = m_id2StateOffsetSize[id].offset;
		auto size = m_id2StateOffsetSize[id].size;

		sim::DefaultBitVectorState newState;
		if (signal.signalRef.driver.node != nullptr)
			newState = m_simulator.getValueOfOutput(signal.signalRef.driver);
		else
			newState = m_simulator.getValueOfInternalState(signal.memory, (size_t) hlim::Node_Memory::Internal::data, signal.memoryWordIdx * signal.memoryWordSize, signal.memoryWordSize);
		if (newState.size() == 0) continue;

		bool stateChanged = false;
		for (auto p : utils::Range(DefaultConfig::NUM_PLANES)) {
			for (auto i : utils::Range(size))
				if (newState.get(p, i) != m_trackedState.get(p, offset+i)) {
					stateChanged = true;
					break;
				}
			if (stateChanged) break;
		}

		if (stateChanged) {
			m_trackedState.copyRange(offset, newState, 0, size);
			signalChanged(id);
		}
	}
}

void WaveformRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
	if (m_initialized)
		advanceTick(simulationTime);
}



}
