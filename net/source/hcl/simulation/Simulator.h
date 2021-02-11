#pragma once

#include "BitVectorState.h"
#include "SimulatorCallbacks.h"

#include "simFiber/SimulationFiber.h"

#include "../hlim/NodeIO.h"
#include "../hlim/Clock.h"
#include "../utils/BitManipulation.h"
#include "../utils/CppTools.h"


#include <vector>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include <coroutine>

namespace hcl::core::hlim {
    class Node_Pin;
}

namespace hcl::core::sim {

class RunTimeSimulationContext;
class WaitFor;
class WaitUntil;

class Simulator
{
    public:
        virtual ~Simulator() = default;

        void addCallbacks(SimulatorCallbacks *simCallbacks) { m_callbackDispatcher.m_callbacks.push_back(simCallbacks); }

        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) = 0;

        virtual void powerOn() = 0;

        virtual void reevaluate() = 0;
        virtual void advanceEvent() = 0;
        virtual void advance(hlim::ClockRational seconds) = 0;

        virtual void setInputPin(hlim::Node_Pin *pin, const DefaultBitVectorState &state) = 0;
        virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) = 0;
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) = 0;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) = 0;
        //virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) = 0;

        inline const hlim::ClockRational &getCurrentSimulationTime() { return m_simulationTime; }

        virtual void addSimulationFiber(std::function<SimulationFiber()> fiber) = 0;

        virtual void simulationFiberSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>) = 0;
        virtual void simulationFiberSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>) = 0;
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
