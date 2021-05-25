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

#include "BitVectorState.h"
#include "../hlim/NodeIO.h"
#include "../utils/BitManipulation.h"

#include <vector>
#include <functional>
#include <map>
#include <queue>
#include <list>

namespace gtry::sim {

struct ClockState {
    bool high;
    hlim::ClockRational nextTrigger;
};

struct DataState
{
    DefaultBitVectorState signalState;
    std::vector<ClockState> clockState;
};

struct StateMapping
{
    std::map<hlim::NodePort, size_t> outputToOffset;
    std::map<hlim::BaseNode*, std::vector<size_t>> nodeToInternalOffset;
    std::map<hlim::Clock*, size_t> clockToClkDomain;
    /*
    std::map<hlim::Clock*, size_t> clockToSignalIdx;
    std::map<hlim::Clock*, size_t> clockToClkIdx;
    */
//    std::map<std::string, size_t> resetToSignalIdx;

    StateMapping() { clear(); }

    void clear() {
        outputToOffset.clear(); outputToOffset[{}] = SIZE_MAX; //clockToSignalIdx.clear(); clockToClkIdx.clear();
        clockToClkDomain.clear();
        //resetToSignalIdx.clear();
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

class HardwareAssert
{
    public:
    protected:
};

class ClockedNode
{
    public:
        ClockedNode(MappedNode mappedNode, size_t clockPort);

        void advance(SimulatorCallbacks &simCallbacks, DataState &state) const;
    protected:
        MappedNode m_mappedNode;
        size_t m_clockPort;
};

struct ClockDomain
{
    std::vector<ClockedNode> clockedNodes;
    std::vector<size_t> dependentExecutionBlocks;
};


/*
struct ClockDriver
{
    size_t srcSignalIdx;
    size_t dstClockIdx;
};
*/
struct Program
{
    void compileProgram(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes);

    size_t m_fullStateWidth;

    StateMapping m_stateMapping;

    std::vector<MappedNode> m_powerOnNodes;
    //std::vector<ClockDriver> m_clockDrivers;
    std::vector<ClockDomain> m_clockDomains;
    std::vector<ExecutionBlock> m_executionBlocks;

    protected:
        void allocateSignals(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes);
};

struct Event {
    enum class Type {
        clock,
        simProcResume
    };
    Type type;
    hlim::ClockRational timeOfEvent;

    struct {
        hlim::Clock *clock;
        size_t clockDomainIdx;
        bool risingEdge;
    } clockEvt;
    struct {
        std::coroutine_handle<> handle;
        std::uint64_t insertionId;
    } simProcResumeEvt;

    bool operator<(const Event &rhs) const {
        if (hlim::clockMore(timeOfEvent, rhs.timeOfEvent)) return true;
        if (hlim::clockLess(timeOfEvent, rhs.timeOfEvent)) return false;
        if ((unsigned)type > (unsigned) rhs.type) return true; // clocks before fibers
        if ((unsigned)type < (unsigned)rhs.type) return false; // clocks before fibers
        if (type == Type::simProcResume)
            return simProcResumeEvt.insertionId > rhs.simProcResumeEvt.insertionId;
        return false;
    }
};

class ReferenceSimulator : public Simulator
{
    public:
        ReferenceSimulator();
        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) override;


        virtual void powerOn() override;
        virtual void reevaluate() override;
        virtual void commitState() override;
        virtual void advanceEvent() override;
        virtual void advance(hlim::ClockRational seconds) override;
        virtual void abort() override { m_abortCalled = true; }

        virtual void simProcSetInputPin(hlim::Node_Pin *pin, const DefaultBitVectorState &state) override;
        virtual DefaultBitVectorState simProcGetValueOfOutput(const hlim::NodePort &nodePort) override;


        virtual bool outputOptimizedAway(const hlim::NodePort &nodePort) override;
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) override;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) override;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) override;
        //virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) override;

        virtual void addSimulationProcess(std::function<SimulationProcess()> simProc) override;

        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>) override;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>) override;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock, utils::RestrictTo<RunTimeSimulationContext>) override;
    protected:
        Program m_program;
        DataState m_dataState;

        std::priority_queue<Event> m_nextEvents;

        std::vector<std::function<SimulationProcess()>> m_simProcs;
        std::list<SimulationProcess> m_runningSimProcs;
        bool m_stateNeedsReevaluating = false;
        std::uint64_t m_nextSimProcInsertionId = 0;

        bool m_currentTimeStepFinished = true;
        bool m_abortCalled = false;
};

}
