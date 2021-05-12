/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

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

#include "../hlim/ClockRational.h"
#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include <string>

namespace gtry::hlim {
    class Clock;
    class BaseNode;
}

namespace gtry::sim {

/**
 * @todo write docs
 */
class SimulatorCallbacks
{
    public:

        virtual void onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) { }
        virtual void onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) { }

        virtual void onNewTick(const hlim::ClockRational &simulationTime) { }
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) { }
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) { }
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) { }
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) { }

        virtual void onSimProcOutputOverridden(hlim::NodePort output, const DefaultBitVectorState &state) { }
        virtual void onSimProcOutputRead(hlim::NodePort output, const DefaultBitVectorState &state) { }
    protected:
};


/**
 * @todo write docs
 */
class SimulatorConsoleOutput : public SimulatorCallbacks
{
    public:
        virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
    protected:
};


}
