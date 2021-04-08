/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "BitVectorState.h"
#include "SimulatorCallbacks.h"

#include "simProc/SimulationProcess.h"

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
class WaitClock;

class Simulator
{
    public:
        virtual ~Simulator() = default;

        void addCallbacks(SimulatorCallbacks *simCallbacks) { m_callbackDispatcher.m_callbacks.push_back(simCallbacks); }

        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) = 0;

        /// Reset circuit and simulation processes into the power-on state
        virtual void powerOn() = 0;

        /// Forces a reevaluation of all combinatorics.
        virtual void reevaluate() = 0;

        /**
         * @brief Advance simulation to the next event
         * @details First moves the simulation time to the next event, then announces the new
         * time tick through SimulatorCallbacks::onNewTick. If the event is a clock event, it first advances the registers of the clock
         * (if the clock is triggering on that edge) and then announces SimulatorCallbacks::onClock.
         * After all registers (or register like node) have advanced, the driven combinatorial networks are evaluated.
         * If any simulation processes resume at the same time, they are always resumed after evaluation of the combinatorics.
         * Finally, if a simulation processes modified any inputs, any subsequent queries of the state from other simulation processes return the new state.
         */
        virtual void advanceEvent() = 0;

        /**
         * @brief Advance simulation by given amount of time or until aborted.
         * @details The equivalent of advancing through all scheduled events and those newly created in the process
         * until all remaining events are in the future of m_simulationTime + seconds or until abort() is called.
         * @param seconds Amount of time (in seconds) by which the simulation gets advanced.
         */
        virtual void advance(hlim::ClockRational seconds) = 0;

        /**
         * @brief Aborts a running simulation mid step
         * @details This immediately aborts calls to advanceEvent() or advance().
         * Time steps are not brought to conclusion, leaving the simulation in a potential mid-step state.
         */
        virtual void abort() = 0;

        virtual void simProcSetInputPin(hlim::Node_Pin *pin, const DefaultBitVectorState &state) = 0;
        virtual DefaultBitVectorState simProcGetValueOfOutput(const hlim::NodePort &nodePort) = 0;

        virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) = 0;
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) = 0;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) = 0;
        //virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) = 0;

        inline const hlim::ClockRational &getCurrentSimulationTime() { return m_simulationTime; }

        virtual void addSimulationProcess(std::function<SimulationProcess()> simProc) = 0;

        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>) = 0;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>) = 0;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock, utils::RestrictTo<RunTimeSimulationContext>) = 0;

        virtual void annotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) { m_callbackDispatcher.onAnnotationStart(simulationTime, id, desc); }
        virtual void annotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) { m_callbackDispatcher.onAnnotationEnd(simulationTime, id); }

    protected:
        class CallbackDispatcher : public SimulatorCallbacks {
            public:
                std::vector<SimulatorCallbacks*> m_callbacks;

                virtual void onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) override;
                virtual void onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) override;

                virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
                virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
                virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
                virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
                virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;

                virtual void onSimProcOutputOverridden(hlim::NodePort output, const DefaultBitVectorState &state) override;
                virtual void onSimProcOutputRead(hlim::NodePort output, const DefaultBitVectorState &state) override;
        };

        hlim::ClockRational m_simulationTime;
        CallbackDispatcher m_callbackDispatcher;
};

}
