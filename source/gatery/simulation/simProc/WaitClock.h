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

namespace gtry::hlim {
    class Clock;
}

namespace gtry::sim {

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
