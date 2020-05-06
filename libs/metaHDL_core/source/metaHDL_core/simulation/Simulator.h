#pragma once

#include "../hlim/NodeIO.h"

#include <vector>
#include <functional>
#include <map>

namespace mhdl::core::sim {

struct DataState 
{
    std::vector<bool> values; // todo: alignment?
    std::vector<bool> definedness; // todo: alignment?
};

struct StateMapping
{
    std::map<hlim::NodePort, size_t> outputToOffset;
    std::map<std::string, size_t> clockToOffset;
    std::map<std::string, size_t> resetToOffset;
};

class ExecutionBlock
{
    public:
        void execute(DataState &state);
    protected:
        std::vector<size_t> m_dependsOnExecutionBlocks;
        std::vector<size_t> m_dependentExecutionBlocks;
        std::vector<std::function<void(DataState &state)>> m_instructions;
};

class Register
{
    public:
        void advance(DataState &state);
    protected:
        size_t m_dataWordSize;
        size_t m_srcDataWordOffset;
        size_t m_dstDataWordOffset;
        size_t m_resetDataWordOffset;
        
        size_t m_dependsOnExecutionBlock;
        std::vector<size_t> m_dependentExecutionBlocks;
};

struct ClockDomain
{
    size_t clkIdx;
    size_t resetIdx;
    std::vector<Register> registers;
};

struct ExecutionState
{
    std::vector<size_t> clocksTriggered;
};


struct ClockDriver 
{
    size_t srcSignalIdx;
    size_t dstClockIdx;
};

class Program
{
    public:
        void executeCombinatory(ExecutionState &executionState, DataState &dataState, bool forceFullReevaluation = false);
        void advanceRegisters(ExecutionState &executionState, DataState &dataState);
    protected:
        DataState m_initialState;
        size_t m_fullStateWidth;
        
        StateMapping m_stateMapping;

        std::vector<std::vector<size_t>> m_clockIdx2clockDomain;
        std::vector<ClockDriver> m_clockDrivers;
        std::vector<ClockDomain> m_clockDomains;
        std::vector<ExecutionBlock> m_executionBlocks;
};
    
class Simulator
{
    public:
        void compileProgram(const hlim::Circuit &circuit);
        
        void reevaluate();
        void advanceTick();
        
        void getValueOfOutput(const hlim::NodePort &nodePort, DataState &value);
        void getValueOfClock(const std::string &clk, bool &value, bool &defined);
        void getValueOfReset(const std::string &reset, bool &value, bool &defined);
    protected:
        Program m_program;
        ExecutionState m_executionState;
        DataState m_dataState;
};

}
