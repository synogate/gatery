#include "WaitUntil.h"

#include "../SimulationContext.h"

#include <iostream>

namespace hcl::core::sim {

WaitUntil::WaitUntil(hlim::NodePort np, Trigger trigger) : m_np(np), m_trigger(trigger)
{

}

void WaitUntil::await_suspend(std::coroutine_handle<> handle)
{
    SimulationContext::current()->simulationFiberSuspending(handle, *this);
}

}