#include "WaitFor.h"

#include "../SimulationContext.h"

#include <iostream>

namespace hcl::core::sim {

WaitFor::WaitFor(const hlim::ClockRational &seconds) : m_seconds(seconds)
{

}

void WaitFor::await_suspend(std::coroutine_handle<> handle)
{
    SimulationContext::current()->simulationFiberSuspending(handle, *this);
}

}