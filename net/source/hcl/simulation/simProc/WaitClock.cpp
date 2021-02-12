#include "WaitClock.h"

#include "../SimulationContext.h"

#include <iostream>

namespace hcl::core::sim {

WaitClock::WaitClock(const hlim::Clock *clock) : m_clock(clock)
{

}

void WaitClock::await_suspend(std::coroutine_handle<> handle)
{
    SimulationContext::current()->simulationProcessSuspending(handle, *this);
}

}