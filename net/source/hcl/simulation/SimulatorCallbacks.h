#pragma once

#include "../hlim/ClockRational.h"

#include <string>

namespace hcl::core::hlim {
    class Clock;
    class BaseNode;
}

namespace hcl::core::sim {

/**
 * @todo write docs
 */
class SimulatorCallbacks
{
    public:
        virtual void onNewTick(const hlim::ClockRational &simulationTime) { }
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) { }
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) { }
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) { }
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) { }
    protected:
};

}
