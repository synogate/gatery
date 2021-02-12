#pragma once


#include <coroutine>

namespace hcl::core::hlim {
    class Clock;
}

namespace hcl::core::sim {

/**
 * @brief co_awaiting on a WaitClock continues the simulation until the clock "activates".
 * @details A clock activation is whatever makes the registeres attached to that clock advance.
 * E.g. depending on the clock configuration a falling edge, a raising edge, or both.
 * If the clock is already in the "activated" state, the simulation continues until it activates again.
 * This means repeatedly co_awaiting a clock can be used to advance in clock ticks.
 */
class WaitClock {
    public:
        WaitClock(const hlim::Clock *clock);

        bool await_ready() noexcept { return false; } // always force reevaluation
        void await_suspend(std::coroutine_handle<> handle);
        void await_resume() noexcept { }

        const hlim::Clock *getClock() { return m_clock; }
    protected:
        const hlim::Clock *m_clock;
};

}