#pragma once

#include <string>

namespace hcl::core::hlim {
    class BaseClock;
    class BaseNode;
}

namespace hcl::core::sim {

/**
 * @todo write docs
 */
class SimulatorCallbacks
{
    public:
        virtual void onNewTick(const hlim::BaseClock *clock) = 0;
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) = 0;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) = 0;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) = 0;
    protected:
};


}
