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

namespace gtry::sim {

Simulator::Simulator()
{

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

}
