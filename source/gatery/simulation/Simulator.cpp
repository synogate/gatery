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
#include "Simulator.h"

#include "../hlim/Circuit.h"
#include "../hlim/Node.h"

#include "../debug/DebugInterface.h"

#include <chrono>

namespace gtry::sim {


void SimulatorPerformanceCounters::reset(const hlim::Circuit &circuit)
{
	m_byNodeGroup.clear();
	m_byNodeType.clear();
	m_typeNameMap.clear();

	for (const auto &n : circuit.getNodes()) {
		m_byNodeGroup[n->getGroup()].active = 0;
		m_byNodeGroup[n->getGroup()].count = 0;
		m_byNodeType[std::type_index(typeid(*n))].active = 0;
		m_byNodeType[std::type_index(typeid(*n))].count = 0;
		m_typeNameMap[std::type_index(typeid(*n))] = typeid(*n).name();
	}

	for (auto &p : m_byOther) {
		p.active = 0;
		p.count = 0;
	}
}

SimulatorPerformanceCounters::NodeHandle::NodeHandle(SimulatorPerformanceCounters &tracker, const hlim::BaseNode *node)
{
	m_groupIt = tracker.m_byNodeGroup.find(node->getGroup());
	m_typeIt = tracker.m_byNodeType.find(std::type_index(typeid(*node)));
	m_groupIt->second.active++;
	m_typeIt->second.active++;
}

SimulatorPerformanceCounters::NodeHandle::~NodeHandle()
{
	m_groupIt->second.active--;
	m_typeIt->second.active--;
}

SimulatorPerformanceCounters::OtherHandle::OtherHandle(SimulatorPerformanceCounters &tracker, Other other)
{
	m_it = tracker.m_byOther.begin() + (size_t) other;
	m_it->active++;
}

SimulatorPerformanceCounters::OtherHandle::~OtherHandle()
{
	m_it->active--;
}


void SimulatorPerformanceCounters::tick()
{
	for (auto &p : m_byNodeGroup)
		if (p.second.active > 0)
			p.second.count++;

	for (auto &p : m_byNodeType)
		if (p.second.active > 0)
			p.second.count++;

	for (auto &p : m_byOther)
		if (p.active > 0)
			p.count++;
}



Simulator::Simulator()
{

}

Simulator::~Simulator()
{
	stopPerformanceCounterThread();
}

void Simulator::CallbackDispatcher::onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc)
{
	for (auto *c : m_callbacks) c->onAnnotationStart(simulationTime, id, desc);
}

void Simulator::CallbackDispatcher::onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id)
{
	for (auto *c : m_callbacks) c->onAnnotationEnd(simulationTime, id);
}

void Simulator::CallbackDispatcher::onPowerOn()
{
	for (auto *c : m_callbacks) c->onPowerOn();
}

void Simulator::CallbackDispatcher::onAfterPowerOn()
{
	for (auto *c : m_callbacks) c->onAfterPowerOn();
}

void Simulator::CallbackDispatcher::onCommitState()
{
	for (auto *c : m_callbacks) c->onCommitState();
}

void Simulator::CallbackDispatcher::onNewTick(const hlim::ClockRational &simulationTime)
{
	for (auto *c : m_callbacks) c->onNewTick(simulationTime);
}

void Simulator::CallbackDispatcher::onNewPhase(size_t phase)
{
	for (auto *c : m_callbacks) c->onNewPhase(phase);
}

void Simulator::CallbackDispatcher::onAfterMicroTick(size_t microTick)
{
	for (auto *c : m_callbacks) c->onAfterMicroTick(microTick);
}

void Simulator::CallbackDispatcher::onClock(const hlim::Clock *clock, bool risingEdge)
{
	for (auto *c : m_callbacks) c->onClock(clock, risingEdge);
}

void Simulator::CallbackDispatcher::onReset(const hlim::Clock *clock, bool resetAsserted)
{
	for (auto *c : m_callbacks) c->onReset(clock, resetAsserted);
}

void Simulator::CallbackDispatcher::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
	for (auto *c : m_callbacks) c->onDebugMessage(src, msg);
}

void Simulator::CallbackDispatcher::onWarning(const hlim::BaseNode *src, std::string msg)
{
	for (auto *c : m_callbacks) c->onWarning(src, msg);
}

void Simulator::CallbackDispatcher::onAssert(const hlim::BaseNode *src, std::string msg)
{
	for (auto *c : m_callbacks) c->onAssert(src, msg);
}

void Simulator::CallbackDispatcher::onSimProcOutputOverridden(const hlim::NodePort &output, const ExtendedBitVectorState &state)
{
	for (auto *c : m_callbacks) c->onSimProcOutputOverridden(output, state);
}

void Simulator::CallbackDispatcher::onSimProcOutputRead(const hlim::NodePort &output, const DefaultBitVectorState &state)
{
	for (auto *c : m_callbacks) c->onSimProcOutputRead(output, state);
}

DefaultBitVectorState Simulator::simProcGetValueOfOutput(const hlim::NodePort &nodePort)
{
	auto value = getValueOfOutput(nodePort);
	m_callbackDispatcher.onSimProcOutputRead(nodePort, value);
	return value;
}


void Simulator::startPerformanceCounterThread(const PerformanceCounterOptions &options)
{
	stopPerformanceCounterThread();

	if (options.samplePerformanceCounters) {
		m_doRunPerformanceCounterThread = true;
		m_performanceCounterThread = std::thread([this, options]{

			auto sleepDuration = std::chrono::duration<float>{ 1.0f / options.performanceCounterSamplingFrequency };

			std::optional<size_t> logUpdateInterval;
			if (options.logPerformanceCounters)
				logUpdateInterval = (size_t)(options.performanceCounterSamplingFrequency / options.performanceCounterLoggingFrequency);

			size_t iter = 0;
			while (m_doRunPerformanceCounterThread) {
				m_performanceCounters.tick();
				std::this_thread::sleep_for(sleepDuration);
				if (logUpdateInterval && ++iter >= *logUpdateInterval) {
					m_performanceCountersNeedWriteback = true;
					iter = 0;
				}
			}
			if (options.logPerformanceCounters)
				m_performanceCountersNeedWriteback = true;
		});
	}
}

void Simulator::stopPerformanceCounterThread()
{
	if (m_doRunPerformanceCounterThread) {
		m_doRunPerformanceCounterThread = false;
		m_performanceCounterThread.join();
		checkWritebackPerformanceCounters();
	}
}

void Simulator::checkWritebackPerformanceCounters()
{
	if (m_performanceCountersNeedWriteback)
		dbg::updateSimulationPerformanceTrace(m_performanceCounters);
}


}
