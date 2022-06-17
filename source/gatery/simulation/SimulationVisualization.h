/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include <span>
#include <type_traits>

namespace gtry::sim {

struct SimulationVisualization
{
    size_t stateSize;
    size_t stateAlignment;
    std::function<void(void*)> reset, capture, render;
};


template<typename State, typename = std::enable_if_t<std::is_same_v<State, void> || std::is_trivially_copyable_v<State>>>
class SimViz {
    public:
        typedef SimViz<State> Self;

        SimulationVisualization stripType() const {
            using namespace std::placeholders;
            SimulationVisualization vis;
            vis.stateSize = sizeof(State);
            vis.stateAlignment = alignof(State);
            if (m_onReset)
                vis.reset = [f = this->m_onReset](void *ptr) { f(*(State*)ptr); };
            if (m_onCapture)
                vis.capture = [f = this->m_onCapture](void *ptr) { f(*(State*)ptr); };
            if (m_onRender)
                vis.render = [f = this->m_onRender](void *ptr) { f(*(State*)ptr); };
            return vis;
        }

        Self &onReset(std::function<void(State&)> f) { m_onReset = std::move(f); return *this; }
        // todo: This needs to be bound to more specific events like clocks.
        Self &onCapture(std::function<void(State&)> f) { m_onCapture = std::move(f); return *this; }
        Self &onRender(std::function<void(State&)> f) { m_onRender = std::move(f); return *this; }
    protected:
        std::function<void(State&)> m_onReset, m_onCapture, m_onRender;
};


template<>
class SimViz<void> {
    public:
        typedef SimViz<void> Self;

        SimulationVisualization stripType() const {
            using namespace std::placeholders;
            SimulationVisualization vis;

            vis.stateSize = 0;
            vis.stateAlignment = 0;

            if (m_onReset)
                vis.reset = std::bind(m_onReset);
            if (m_onCapture)
                vis.capture = std::bind(m_onCapture);
            if (m_onRender)
                vis.render = std::bind(m_onRender);
            return vis;
        }

        Self &onReset(std::function<void()> f) { m_onReset = std::move(f); return *this; }
        Self &onCapture(std::function<void()> f) { m_onCapture = std::move(f); return *this; }
        Self &onRender(std::function<void()> f) { m_onRender = std::move(f); return *this; }
    protected:
        std::function<void()> m_onReset, m_onCapture, m_onRender;
};



}