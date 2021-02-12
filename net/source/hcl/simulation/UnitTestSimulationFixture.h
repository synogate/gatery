#pragma once

#include "SimulatorCallbacks.h"

#include <memory>

namespace hcl::core::hlim {
    class Circuit;
}

namespace hcl::core::sim {

    class Simulator;
    class SimulationProcess;

/**
 * @todo write docs
 */
class UnitTestSimulationFixture : public SimulatorCallbacks
{
    public:
        UnitTestSimulationFixture();
        ~UnitTestSimulationFixture();

        void addSimulationProcess(std::function<SimulationProcess()> simProc);

        void eval(hlim::Circuit &circuit);
        void runTicks(hlim::Circuit &circuit, const hlim::Clock *clock, unsigned numTicks);

        virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
    protected:
        std::unique_ptr<Simulator> m_simulator;

        const hlim::Clock *m_runLimClock = nullptr;
        unsigned m_runLimTicks = 0;
};


}
