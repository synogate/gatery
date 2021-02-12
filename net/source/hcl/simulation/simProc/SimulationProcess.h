#pragma once

#include <coroutine>

namespace hcl::core::sim {

class SimulationProcess {
    public:
        struct promise_type {
            auto get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
            auto initial_suspend() { return std::suspend_always(); }
            auto final_suspend() noexcept { return std::suspend_always(); }
            void return_void() { }
            void unhandled_exception() { throw; }
        };
        using Handle = std::coroutine_handle<promise_type>;

        SimulationProcess(Handle handle);
        ~SimulationProcess();

        SimulationProcess(const SimulationProcess&) = delete;
        SimulationProcess(SimulationProcess &&rhs) { m_handle = rhs.m_handle; rhs.m_handle = {}; }
        void operator=(const SimulationProcess&) = delete;

        void resume();
    protected:
        Handle m_handle;
};

}