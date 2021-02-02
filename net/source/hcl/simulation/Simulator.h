#pragma once

#include "../hlim/NodeIO.h"
#include "../hlim/Clock.h"
#include "BitVectorState.h"
#include "../utils/BitManipulation.h"
#include "SimulatorCallbacks.h"


#include <vector>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace hcl::core::sim {

class Simulator
{
    public:
        virtual ~Simulator() = default;

        void addCallbacks(SimulatorCallbacks *simCallbacks) { m_callbackDispatcher.m_callbacks.push_back(simCallbacks); }
        
        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) = 0;
        
        virtual void reset() = 0;
        
        virtual void reevaluate() = 0;
        virtual void advanceAnyTick() = 0;
        
        virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) = 0;
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) = 0;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) = 0;
    
        inline const hlim::ClockRational &getCurrentSimulationTime() { return m_simulationTime; }
    protected:
        class CallbackDispatcher : public SimulatorCallbacks {
            public:
                std::vector<SimulatorCallbacks*> m_callbacks;

                virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
                virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
                virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
                virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
                virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
        };

        hlim::ClockRational m_simulationTime;
        CallbackDispatcher m_callbackDispatcher;
};

}
