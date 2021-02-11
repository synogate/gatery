#pragma once

#include <coroutine>

namespace hcl::core::sim {

class SimulationFiber {
    public:
        struct promise_type {
            auto get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
            auto initial_suspend() { return std::suspend_always(); }
            auto final_suspend() noexcept { return std::suspend_always(); }
            void return_void() { }
            void unhandled_exception() {} // todo
        };
        using Handle = std::coroutine_handle<promise_type>;

        SimulationFiber(Handle handle);
        ~SimulationFiber();

        SimulationFiber(const SimulationFiber&) = delete;
        SimulationFiber(SimulationFiber &&rhs) { m_handle = rhs.m_handle; rhs.m_handle = {}; }
        void operator=(const SimulationFiber&) = delete;

        void resume();
    protected:
        Handle m_handle;
};

}