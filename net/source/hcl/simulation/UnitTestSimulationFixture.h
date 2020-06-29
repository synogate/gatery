#pragma once

#include "SimulatorCallbacks.h"

#include <memory>

namespace hcl::core::hlim {
    class Circuit;
}

namespace hcl::core::sim {
    
    class Simulator;

/**
 * @todo write docs
 */
class UnitTestSimulationFixture : public SimulatorCallbacks
{
    public:
        UnitTestSimulationFixture();
        ~UnitTestSimulationFixture();
        
        void eval(const hlim::Circuit &circuit);
        void runTicks(const hlim::Circuit &circuit, const hlim::BaseClock *clock, unsigned numTicks);

        virtual void onNewTick(const hlim::BaseClock *clock) override;
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
    protected:
        std::unique_ptr<Simulator> m_simulator;
        
        const hlim::BaseClock *m_runLimClock = nullptr;
        unsigned m_runLimTicks = 0;
};


}
