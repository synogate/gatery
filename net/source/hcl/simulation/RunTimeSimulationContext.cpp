/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "RunTimeSimulationContext.h"

#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/supportNodes/Node_SignalTap.h"

#include "Simulator.h"

namespace hcl::core::sim {

RunTimeSimulationContext::RunTimeSimulationContext(Simulator *simulator) : m_simulator(simulator)
{
}

void RunTimeSimulationContext::overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state)
{
    if (dynamic_cast<hlim::Node_Signal*>(output.node))
        output = output.node->getNonSignalDriver(0);
    auto *pin = dynamic_cast<hlim::Node_Pin*>(output.node);
    HCL_DESIGNCHECK_HINT(pin != nullptr, "Only io pin outputs allow run time overrides!");
    m_simulator->simProcSetInputPin(pin, state);
}

void RunTimeSimulationContext::getSignal(hlim::NodePort output, DefaultBitVectorState &state)
{
    /*
    // Check doesn't work because one connects to the output driving the outputPin
    auto *sigTap = dynamic_cast<hlim::Node_SignalTap*>(output.node);
    auto *pin = dynamic_cast<hlim::Node_Pin*>(output.node);
    HCL_DESIGNCHECK_HINT(pin || sigTap, "Only io pins and signal taps allow run time reading!");
    */
    state = m_simulator->simProcGetValueOfOutput(output);
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
