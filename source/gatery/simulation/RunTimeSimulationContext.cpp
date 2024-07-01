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
#include "RunTimeSimulationContext.h"
#include "SigHandle.h"

#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/supportNodes/Node_SignalTap.h"
#include "../hlim/GraphTools.h"

#include "Simulator.h"

#include <set>

namespace gtry::sim {

RunTimeSimulationContext::RunTimeSimulationContext(Simulator *simulator) : m_simulator(simulator)
{
}

void RunTimeSimulationContext::overrideSignal(const SigHandle &handle, const ExtendedBitVectorState &state)
{
	if (state.size() == 0)
		return;

	hlim::Node_Pin *pin = nullptr;
	
	auto it = m_sigOverridePinCache.find(handle.getOutput());
	if (it == m_sigOverridePinCache.end()) {
		pin = hlim::findInputPin(handle.getOutput());
		m_sigOverridePinCache[handle.getOutput()] = pin;
	} else
		pin = it->second;
	HCL_DESIGNCHECK_HINT(pin != nullptr, "Only io pin inputs allow run time overrides, but none was found!");
	m_simulator->simProcSetInputPin(pin, state);
}

void RunTimeSimulationContext::overrideRegister(const SigHandle &handle, const DefaultBitVectorState &state)
{
	if (state.size() == 0)
		return;

	auto *node = handle.getOutput().node;
	if (dynamic_cast<hlim::Node_Signal*>(node))
		node = node->getNonSignalDriver(0).node;

	auto *reg = dynamic_cast<hlim::Node_Register*>(node);
	HCL_DESIGNCHECK_HINT(reg != nullptr, "Trying to override register output, but the signal is not driven by a register.");
	m_simulator->simProcOverrideRegisterOutput(reg, state);
}

void RunTimeSimulationContext::getSignal(const SigHandle &handle, DefaultBitVectorState &state)
{
	state = m_simulator->simProcGetValueOfOutput(handle.getOutput());
}

void RunTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor)
{
	m_simulator->simulationProcessSuspending(handle, waitFor, {});
}

void RunTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil)
{
	m_simulator->simulationProcessSuspending(handle, waitUntil, {});
}

void RunTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock)
{
	m_simulator->simulationProcessSuspending(handle, waitClock, {});
}

void RunTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitChange &waitChange)
{
	m_simulator->simulationProcessSuspending(handle, waitChange, {});
}

void RunTimeSimulationContext::simulationProcessSuspending(std::coroutine_handle<> handle, WaitStable &waitStable)
{
	m_simulator->simulationProcessSuspending(handle, waitStable, {});
}

void RunTimeSimulationContext::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
	m_simulator->onDebugMessage(src, std::move(msg));
}

void RunTimeSimulationContext::onWarning(const hlim::BaseNode *src, std::string msg)
{
	m_simulator->onWarning(src, std::move(msg));
}

void RunTimeSimulationContext::onAssert(const hlim::BaseNode *src, std::string msg)
{
	m_simulator->onAssert(src, std::move(msg));
}

bool RunTimeSimulationContext::hasAuxData(std::string_view key) const
{
	return m_simulator->hasAuxData(key);
}

std::any& RunTimeSimulationContext::registerAuxData(std::string_view key, std::any data)
{
	return m_simulator->registerAuxData(key, std::move(data));
}

std::any& RunTimeSimulationContext::getAuxData(std::string_view key)
{
	return m_simulator->getAuxData(key);
}





}
