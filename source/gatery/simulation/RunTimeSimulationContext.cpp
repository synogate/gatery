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
        pin = findInputPin(handle.getOutput());
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


hlim::Node_Pin *RunTimeSimulationContext::findInputPin(hlim::NodePort output) const
{
    // Check if the driver is actually an input pin
    hlim::Node_Pin* res;
    if (res = dynamic_cast<hlim::Node_Pin*>(output.node))
        return res;

    // Otherwise follow signal nodes until an input pin is found.

    // The driver must be a signal node
    HCL_DESIGNCHECK(output.node != nullptr);
    if (dynamic_cast<const hlim::Node_Signal*>(output.node) == nullptr)
        return nullptr;

    std::set<hlim::BaseNode*> encounteredNodes;

    // Any preceeding node must be a signal node or the pin we look for
    for (auto nh : output.node->exploreInput(0)) {
        if (encounteredNodes.contains(nh.node())) {
            nh.backtrack();
            continue;
        }
        encounteredNodes.insert(nh.node());
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else // If we hit s.th. that is neither pin nor signal, there is nothing we can do.
            if (!nh.isSignal())
                return nullptr;
    }
    return nullptr;
}


hlim::Node_Pin *RunTimeSimulationContext::findOutputPin(hlim::NodePort output) const
{
    // Explore the local graph, traveling along all signal nodes to find any
    // output pin that is driven by whatever drives (directly or indirectly) output. 
    // All such output pins (if there are multiple) recieve the same signal and
    // are thus equivalent, so we can just pick any.

    HCL_DESIGNCHECK(output.node != nullptr);
    // First: Find the non-signal driver that drives output:
    hlim::NodePort driver;
    if (dynamic_cast<const hlim::Node_Signal*>(output.node) != nullptr)
        driver = output;
    else
        driver = output.node->getNonSignalDriver(0);

    // Second: From there on explore all nodes driven directly or via signal nodes.
    for (auto nh : driver.node->exploreOutput(driver.port)) {
        hlim::Node_Pin* res;
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else
            if (!nh.isSignal())
                nh.backtrack();
    }

    return nullptr;
}



}
