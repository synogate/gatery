/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#pragma once

#include <coroutine>

namespace hcl::sim {

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
