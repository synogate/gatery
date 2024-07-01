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

#include "Simulator.h"
#include "simProc/SimulationFiber.h"

#include <gatery/utils/StableContainers.h>
#include "simProc/WaitClock.h"
#include "BitVectorState.h"
#include "../hlim/NodeIO.h"
#include "../utils/BitManipulation.h"
#include "../hlim/postprocessing/ClockPinAllocation.h"
#include "../hlim/Subnet.h"
#include "simProc/SensitivityList.h"

#include <vector>
#include <functional>
#include <map>
#include <queue>
#include <list>

namespace gtry::hlim {
	class Node_Register;
}

namespace gtry::sim {

struct ClockState {
	bool high;
	//hlim::ClockRational nextTrigger;
};

struct ResetState {
	bool resetHigh;
};

struct DataState
{
	DefaultBitVectorState signalState;
	std::vector<ClockState> clockState;
	std::vector<ResetState> resetState;
	std::map<std::string, std::any, std::less<>> auxData;
};

struct StateMapping
{
	utils::UnstableMap<hlim::NodePort, size_t> outputToOffset;
	utils::UnstableMap<hlim::BaseNode*, std::vector<size_t>> nodeToInternalOffset;
	hlim::ClockPinAllocation clockPinAllocation;

	StateMapping() { clear(); }

	void clear() {
		outputToOffset.clear(); outputToOffset[{}] = SIZE_MAX; //clockToSignalIdx.clear(); clockToClkIdx.clear();
	}
};

struct MappedNode {
	hlim::BaseNode *node = nullptr;
	std::vector<size_t> internal;
	std::vector<size_t> inputs;
	std::vector<size_t> outputs;
};

class ExecutionBlock
{
	public:
		void evaluate(SimulatorCallbacks &simCallbacks, DataState &state) const;
		void commitState(SimulatorCallbacks &simCallbacks, DataState &state) const;

		void addStep(MappedNode mappedNode);
	protected:
		//std::vector<size_t> m_dependsOnExecutionBlocks;
		//std::vector<size_t> m_dependentExecutionBlocks;
		std::vector<MappedNode> m_steps;
};

class ClockedNode
{
	public:
		ClockedNode(MappedNode mappedNode, size_t clockPort);

		void clockValueChanged(SimulatorCallbacks &simCallbacks, DataState &state, bool clockValue, bool clockDefined) const;
		void advance(SimulatorCallbacks &simCallbacks, DataState &state) const;
		void changeReset(SimulatorCallbacks &simCallbacks, DataState &state, bool resetHigh) const;
	protected:
		MappedNode m_mappedNode;
		size_t m_clockPort;
};


struct ClockAwaitingSimProc {
	std::uint64_t sortId;
	WaitClock::TimingPhase timingPhase;
	std::coroutine_handle<> handle;
};

/**
 * @brief All nodes driven by a particular clock.
 */
struct ClockDomain
{
	hlim::Clock *clock = nullptr;
	size_t clockSourceIdx = ~0ull;
	size_t resetSourceIdx = ~0ull;
	std::vector<ClockedNode> clockedNodes;
	std::vector<size_t> dependentExecutionBlocks;

	std::vector<ClockAwaitingSimProc> awaitingSimProcs;
};

/**
 * @brief Definition of a combined source for either a clock or reset
 * 
 */
struct ClockPin 
{
	/// Clock in the hierarchy whose clock/reset signal is used for all connected ClockDomains
	hlim::Clock *pin;
	/// If this clock/reset is driven by a signal, the index into the state vector of that signal.
	size_t srcSignalIdx = ~0ull; 
	/// All the domains affected by this source.
	std::vector<ClockDomain*> domains;
};

struct Program
{
	void compileProgram(const hlim::Circuit &circuit, const hlim::Subnet &nodes);

	size_t m_fullStateWidth = 0;

	StateMapping m_stateMapping;

	std::vector<MappedNode> m_powerOnNodes;
	std::vector<ClockPin> m_clockSources;
	std::vector<ClockPin> m_resetSources;
	utils::UnstableMap<hlim::Clock*, ClockDomain> m_clockDomains;
	std::vector<ExecutionBlock> m_executionBlocks;

	protected:
		void allocateSignals(const hlim::Circuit &circuit, const hlim::Subnet &nodes);
		void allocateClocks(const hlim::Circuit &circuit, const hlim::Subnet &nodes);
};

struct Event {
	enum class Type {
		clockPinTrigger,
		simProcResume,
		clockValueChange,
		resetValueChange,
	};
	Type type = Type::clockPinTrigger;
	hlim::ClockRational timeOfEvent = {0};
	size_t microTick = 0;
	WaitClock::TimingPhase timingPhase = WaitClock::DURING;

	struct ClockValueChangeEvt {
		size_t clockPinIdx = ~0ull;
		bool risingEdge = false;
	};
	struct ResetValueChangeEvt {
		size_t resetPinIdx = ~0ull;
		bool newResetHigh = false;
	};
	struct SimProcResumeEvt {
		std::coroutine_handle<> handle;
		std::uint64_t insertionId = ~0ull;
	};

	std::variant<ClockValueChangeEvt, ResetValueChangeEvt, SimProcResumeEvt> data = ClockValueChangeEvt{};

	template<typename T>
	T &evt() { return std::get<T>(data); }
	template<typename T>
	const T &evt() const { return std::get<T>(data); }

	bool operator<(const Event &rhs) const {
		if (hlim::clockMore(timeOfEvent, rhs.timeOfEvent)) return true;
		if (hlim::clockLess(timeOfEvent, rhs.timeOfEvent)) return false;
		if (timingPhase > rhs.timingPhase) return true;
		if (timingPhase < rhs.timingPhase) return false;
		if (microTick > rhs.microTick) return true;
		if (microTick < rhs.microTick) return false;
		if ((unsigned)type > (unsigned) rhs.type) return true; // fibers before clocks
		if ((unsigned)type < (unsigned)rhs.type) return false; // fibers before clocks
		if (type == Type::simProcResume)
			return evt<SimProcResumeEvt>().insertionId > rhs.evt<SimProcResumeEvt>().insertionId;
		return false;
	}
};

struct SignalWatch {
	struct Signal {
		size_t refStateIdx = ~0ull;
		size_t stateIdx = ~0ull;
		size_t size = ~0ull;
	};
	std::vector<Signal> signals;
	sim::DefaultBitVectorState refState;
	std::coroutine_handle<> handle;
	std::uint64_t insertionId;

	SignalWatch(std::coroutine_handle<> handle, const SensitivityList &list, const StateMapping &stateMapping, const sim::DefaultBitVectorState &state, std::uint64_t insertionId);

	bool anySignalChanged(const sim::DefaultBitVectorState &state) const;

	bool operator<(const SignalWatch &rhs) const {
		return insertionId > rhs.insertionId;
	}
};

class ReferenceSimulator : public Simulator
{
	public:
		ReferenceSimulator(bool enableConsoleOutput = true);
		virtual ~ReferenceSimulator();
		virtual void compileProgram(const hlim::Circuit &circuit, const utils::StableSet<hlim::NodePort> &outputs = {}, bool ignoreSimulationProcesses = false) override;
		void compileStaticEvaluation(const hlim::Circuit& circuit, const utils::StableSet<hlim::NodePort>& outputs);


		virtual void powerOn() override;
		virtual void reevaluate() override;
		virtual void commitState() override;
		virtual void advanceEvent() override;
		virtual void advance(hlim::ClockRational seconds) override;
		virtual void abort() override { m_abortCalled = true; }
		virtual bool abortCalled() const override { return m_abortCalled; }

		virtual bool simulationIsShuttingDown() const override { return m_simulationIsShuttingDown; }

		virtual void simProcSetInputPin(hlim::Node_Pin *pin, const ExtendedBitVectorState &state) override;
		virtual void simProcOverrideRegisterOutput(hlim::Node_Register *reg, const DefaultBitVectorState &state) override;

		virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) override;
		virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx, size_t offset = 0, size_t size = ~0ull) override;
		virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) override;
		virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) override;
		virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const hlim::Clock *clk) override;

		virtual void addSimulationProcess(std::function<SimulationFunction<void>()> simProc) override;
		virtual void addSimulationFiber(std::function<void()> simFiber) override;
		virtual void addSimulationVisualization(sim::SimulationVisualization simVis) override;

		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>) override;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>) override;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock, utils::RestrictTo<RunTimeSimulationContext>) override;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitChange &waitChange, utils::RestrictTo<RunTimeSimulationContext>) override;
		virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitStable &waitStable, utils::RestrictTo<RunTimeSimulationContext>) override;

		virtual bool hasAuxData(std::string_view key) const override;
		virtual std::any& registerAuxData(std::string_view key, std::any data) override;
		virtual std::any& getAuxData(std::string_view key) override;
	protected:
		Program m_program;
		DataState m_dataState;
		std::vector<std::uint64_t> m_simVizStates;
		std::vector<size_t> m_simVizStateOffsets;

		std::priority_queue<Event> m_nextEvents;

		void destroyPendingEvents();


		SimulationCoroutineHandler m_coroutineHandler;

		std::vector<std::coroutine_handle<>> m_processesAwaitingCommit;
		std::vector<std::function<SimulationFunction<>()>> m_simProcs;
		std::vector<std::function<void()>> m_simFiberBodies;
		std::list<SimulationFiber> m_simFibers;
		std::vector<sim::SimulationVisualization> m_simViz;
		std::list<SignalWatch> m_signalWatches;
		bool m_stateNeedsReevaluating = false;
		std::uint64_t m_nextSimProcInsertionId = 0;

		bool m_abortCalled = false;
		bool m_readOnlyMode = false;
		bool m_simulationIsShuttingDown = false;


		struct PerformanceStats {
			std::uint64_t totalRuntimeUs = 0;
			std::uint64_t numReEvals = 0;
			size_t totalRuntimeNumEvents = 0;

			size_t thisEventNumReEvals = 0;
		};

		PerformanceStats m_performanceStats;

		std::optional<SimulatorConsoleOutput> m_simulatorConsoleOutput;

		void advanceMicroTick();
		void checkSignalWatches();
		void handleCurrentTimeStep();

		virtual void startCoroutine(SimulationFunction<void> coroutine) override;
};

}
