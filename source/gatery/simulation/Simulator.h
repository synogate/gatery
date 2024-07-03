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

#include <gatery/utils/StableContainers.h>

#include "BitVectorState.h"
#include "SimulatorCallbacks.h"

#include "simProc/SimulationProcess.h"
#include "simProc/WaitClock.h"
#include "SimulationVisualization.h"

#include "../hlim/NodeIO.h"
#include "../hlim/Clock.h"
#include "../utils/BitManipulation.h"
#include "../utils/CppTools.h"


#include <vector>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include <any>

#include "../compat/CoroutineWrapper.h"

namespace gtry::hlim {
	class Node_Pin;
	class Node_Register;
}

namespace gtry::sim {

class RunTimeSimulationContext;
class WaitFor;
class WaitUntil;
class WaitClock;
class WaitChange;
class WaitStable;

/**
 * @brief Interface for all logic simulators
 * 
 */
class Simulator
{
	public:
		Simulator();
		virtual ~Simulator() = default;

		/// Adds a simulator callback hook to inform waveform recorders and test bench exporters about simulation events.
		void addCallbacks(SimulatorCallbacks *simCallbacks) { m_callbackDispatcher.m_callbacks.push_back(simCallbacks); }

		/**
		 * @brief Prepares the simulator for the simulation of the given circuit.
		 * 
		 * @param circuit The circuit which is to be simulated.
		 * @param outputs Unless left empty, confines simulation to that part of the circuit that has an influence on the given outputs.
		 * @param ignoreSimulationProcesses Wether or not to bring in simulation processes that were stored in the circuit itself.
		 */
		virtual void compileProgram(const hlim::Circuit &circuit, const utils::StableSet<hlim::NodePort> &outputs = {}, bool ignoreSimulationProcesses = false) = 0;

		/** 
			@name Simulator control
		 	@{
		*/

		/// Reset circuit and simulation processes into the power-on state
		virtual void powerOn() = 0;

		/// Forces a reevaluation of all combinatorics.
		virtual void reevaluate() = 0;

		/// Declare current state the final state for this time step. 
		/// @details Evaluates asserts, triggers waveform recorders, etc.
		virtual void commitState() = 0;

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

		template<typename ReturnValue>
		ReturnValue executeCoroutine(SimulationFunction<ReturnValue> coroutine);

		template<typename ReturnValue>
		ReturnValue executeCoroutine(std::function<SimulationFunction<ReturnValue>()> coroutine) { return executeCoroutine(coroutine()); }

		/**
		 * @brief Aborts a running simulation mid step
		 * @details This immediately aborts calls to advanceEvent() or advance().
		 * Time steps are not brought to conclusion, leaving the simulation in a potential mid-step state.
		 */
		virtual void abort() = 0;

		/**
		 * @return returns whether abort() has been called
		*/
		virtual bool abortCalled() const = 0;

		/// @}

		/** 
			@name Simulator IO
		 	@{
		*/

		/// Sets the value of an input pin
		virtual void simProcSetInputPin(hlim::Node_Pin *pin, const ExtendedBitVectorState &state) = 0;
		/// Overrides the output of a register until its next activation
		virtual void simProcOverrideRegisterOutput(hlim::Node_Register *reg, const DefaultBitVectorState &state) = 0;
		/// Returns @ref getValueOfOutput but also notifies potential testbench exporters via @ref SimulatorCallbacks of the "sampling" of this output.
		virtual DefaultBitVectorState simProcGetValueOfOutput(const hlim::NodePort &nodePort);

		virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) = 0;
		virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx, size_t offset = 0, size_t size = ~0ull) = 0;
		virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) = 0;
		virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) = 0;
		virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const hlim::Clock *clk) = 0;

		/// @}

		/// Returns the elapsed simulation time (in seconds) since @ref powerOn.
		inline const hlim::ClockRational &getCurrentSimulationTime() const { return m_simulationTime; }

		/// @brief Returns true in the time period where the simulator is pulling down all the simulation coroutines on reseting or closing the simulation.
		/// @details This allows coroutine code to differentiate between destructing because of going out of scope normally and destructing because the
		///			 entire coroutine stack is being destructed.
		virtual bool simulationIsShuttingDown() const = 0;

		/// Returns the elapsed micro ticks (reevaluations) within the current time step.
		inline size_t getCurrentMicroTick() const { return m_microTick; }
		/// Returns the current timing phase (eg. before registers at that time point trigger, while they trigger, or after they have triggered).
		inline WaitClock::TimingPhase getCurrentPhase() const { return m_timingPhase; }

		/// Adds a simulation process to this simulator that gets started on power on.
		virtual void addSimulationProcess(std::function<SimulationFunction<void>()> simProc) = 0;
		virtual void addSimulationFiber(std::function<void()> simFiber) = 0;
		virtual void addSimulationVisualization(sim::SimulationVisualization simVis) = 0;

		/// Spawns a simulation process immediately (to be called during simulation to "fork").
		//virtual SimulationProcessHandle fork(std::function<SimulationFunction<void>> simProc, utils::RestrictTo<RunTimeSimulationContext>) = 0;

		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock, utils::RestrictTo<RunTimeSimulationContext>) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitChange &waitChange, utils::RestrictTo<RunTimeSimulationContext>) = 0;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitStable &waitStable, utils::RestrictTo<RunTimeSimulationContext>) = 0;

		virtual bool hasAuxData(std::string_view key) const = 0;
		virtual std::any& registerAuxData(std::string_view key, std::any data) = 0;
		virtual std::any& getAuxData(std::string_view key) = 0;

		void onDebugMessage(const hlim::BaseNode *src, std::string msg) { m_callbackDispatcher.onDebugMessage(src, std::move(msg)); }
		void onWarning(const hlim::BaseNode *src, std::string msg) { m_callbackDispatcher.onWarning(src, std::move(msg)); }
		void onAssert(const hlim::BaseNode *src, std::string msg) { m_callbackDispatcher.onAssert(src, std::move(msg)); }

		// virtual void stopSimulationProcess(const SimulationProcessHandle &handle, utils::RestrictTo<SimulationProcessHandle>) = 0;
		// virtual bool simulationProcessHasFinished(const SimulationProcessHandle &handle, utils::RestrictTo<SimulationProcessHandle>) = 0;
//		virtual void suspendUntilProcessCompletion(std::coroutine_handle<> handle, const SimulationProcessHandle &procHandle, utils::RestrictTo<SimulationProcessHandle>) = 0;

		virtual void annotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) { m_callbackDispatcher.onAnnotationStart(simulationTime, id, desc); }
		virtual void annotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) { m_callbackDispatcher.onAnnotationEnd(simulationTime, id); }
	protected:
		class CallbackDispatcher : public SimulatorCallbacks {
			public:
				std::vector<SimulatorCallbacks*> m_callbacks;

				virtual void onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) override;
				virtual void onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) override;

				virtual void onPowerOn() override;
				virtual void onAfterPowerOn() override;
				virtual void onCommitState() override;
				virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
				virtual void onNewPhase(size_t phase) override;
				virtual void onAfterMicroTick(size_t microTick) override;
				virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
				virtual void onReset(const hlim::Clock *clock, bool resetAsserted) override;
				virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
				virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
				virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;

				virtual void onSimProcOutputOverridden(const hlim::NodePort &output, const ExtendedBitVectorState &state) override;
				virtual void onSimProcOutputRead(const hlim::NodePort &output, const DefaultBitVectorState &state) override;
		};

		hlim::ClockRational m_simulationTime;
		size_t m_microTick = 0;
		WaitClock::TimingPhase m_timingPhase;
		CallbackDispatcher m_callbackDispatcher;

		std::mutex m_mutex;

		virtual void startCoroutine(SimulationFunction<void> coroutine) = 0;
};


template<typename ReturnValue>
ReturnValue Simulator::executeCoroutine(SimulationFunction<ReturnValue> coroutine)
{
	std::unique_lock lock(m_mutex);

	ReturnValue result;
	bool done = false;

	auto callbackWrapper = [&result, &done, &coroutine, this]()mutable->SimulationFunction<void> {
		result = co_await coroutine;
		done = true;
	};

	startCoroutine(callbackWrapper());
	while (!done) advanceEvent();

	return result;
}


}
