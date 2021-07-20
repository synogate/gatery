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
#include "../hlim/supportNodes/Node_SignalTap.h"
#include "../hlim/GraphTools.h"

#include "Simulator.h"

#include <set>

namespace gtry::sim {

RunTimeSimulationContext::RunTimeSimulationContext(Simulator *simulator) : m_simulator(simulator)
{
}

void RunTimeSimulationContext::overrideSignal(const SigHandle &handle, const DefaultBitVectorState &state)
{
    hlim::Node_Pin *pin = nullptr;
    
    auto it = m_sigOverridePinCache.find(handle.getOutput());
    if (it == m_sigOverridePinCache.end()) {
        pin = hlim::findInputPin(handle.getOutput());
        m_sigOverridePinCache[handle.getOutput()] = pin;
    } else
        pin = it->second;
    HCL_DESIGNCHECK_HINT(pin != nullptr, "Only io pin outputs allow run time overrides, but none was found!");
    m_simulator->simProcSetInputPin(pin, state);
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




}
