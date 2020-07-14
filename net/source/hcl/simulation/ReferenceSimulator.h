#pragma once

#include "SimulatorCallbacks.h"
#include "Simulator.h"

#include "BitVectorState.h"
#include "../hlim/NodeIO.h"
#include "../utils/BitManipulation.h"

#include <vector>
#include <functional>
#include <map>

namespace hcl::core::sim {

struct DataState 
{
    DefaultBitVectorState signalState;
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

class LatchedNode
{
    public:
        LatchedNode(MappedNode mappedNode, size_t clockPort);
        
        void advance(SimulatorCallbacks &simCallbacks, DataState &state) const;
    protected:
        MappedNode m_mappedNode;
        size_t m_clockPort;
        
        //std::vector<size_t> m_dependentExecutionBlocks;
};

struct ClockDomain
{
    std::vector<LatchedNode> latches;
};

struct ExecutionState
{
    size_t simulationTick = 0;
    std::vector<size_t> clocksTriggered;
};

/*
struct ClockDriver
{
    size_t srcSignalIdx;
    size_t dstClockIdx;
};
*/
class Program
{
    public:
        void compileProgram(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes);

        void reset(SimulatorCallbacks &simCallbacks, DataState &dataState) const;
        void reevaluate(SimulatorCallbacks &simCallbacks, DataState &dataState) const;
        void advanceClock(SimulatorCallbacks &simCallbacks, DataState &dataState, hlim::Clock *clock) const;
        
        
        inline size_t getFullStateWidth() const { return m_fullStateWidth; }
        inline const StateMapping &getStateMapping() const { return m_stateMapping; }
        inline const std::vector<ExecutionBlock> &getExecutionBlocks() const { return m_executionBlocks; }
    protected:
        size_t m_fullStateWidth;
        
        StateMapping m_stateMapping;

        std::vector<MappedNode> m_resetNodes;
        //std::vector<ClockDriver> m_clockDrivers;
        std::vector<ClockDomain> m_clockDomains;
        std::vector<ExecutionBlock> m_executionBlocks;
        
        void allocateSignals(const hlim::Circuit &circuit, const std::vector<hlim::BaseNode*> &nodes);
};
    
class ReferenceSimulator : public Simulator
{
    public:
        ReferenceSimulator(SimulatorCallbacks &simCallbacks);
        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) override;
        
        virtual void reset() override;
        virtual void reevaluate() override;
        virtual void advanceAnyTick() override;
        
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) override;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) override;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::Clock *clk) override;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) override;
    protected:
        SimulatorCallbacks &m_simCallbacks;
        Program m_program;
        ExecutionState m_executionState;
        DataState m_dataState;
        
        hlim::Clock *clk;
};

}
