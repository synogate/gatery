#pragma once

#include "../../hlim/ClockRational.h"

#include <coroutine>

namespace hcl::core::sim {

/**
 * @brief co_awaiting on a WaitFor continues the simulation for the specified amount of seconds.
 * @details After the specified amount of time has passed, the coroutine resumes execution and can access the new values.
 * Waiting for zero seconds forces a reevaluation of the combinatory networks.
 */
class WaitFor {
    public:
        WaitFor(const hlim::ClockRational &seconds);

        bool await_ready() noexcept { return false; } // always force reevaluation
        void await_suspend(std::coroutine_handle<> handle);
        void await_resume() noexcept { }

        hlim::ClockRational getDuration() const { return m_seconds; }
    protected:
        hlim::ClockRational m_seconds;
};

}