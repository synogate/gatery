#pragma once

#include "Simulator.h"

#include "../hlim/NodeIO.h"
#include "../utils/BitManipulation.h"

#include <vector>
#include <functional>
#include <map>

namespace mhdl::core::sim {

class DataState 
{
    public:
        void resize(size_t size);
        inline size_t size() const { return m_size; }
        inline size_t getNumBlocks() const { return m_values.size(); }
        void clear();
        
        bool bit(size_t idx) { return m_values[idx/64] & (1ull << (idx % 64)); }
        bool defined(size_t idx) { return m_defined[idx/64] & (1ull << (idx % 64)); }

        void setBit(size_t idx, bool value) { if (value) m_values[idx/64] |= (1ull << (idx % 64)); else m_values[idx/64] &= ~(1ull << (idx % 64)); }
        void setDefined(size_t idx, bool value) { if (value) m_defined[idx/64] |= (1ull << (idx % 64)); else m_defined[idx/64] &= ~(1ull << (idx % 64)); }
        
        DataState getRange(size_t offset, size_t size);
        
        std::uint64_t *getValueData() { return m_values.data(); }
        const std::uint64_t *getValueData() const { return m_values.data(); }
        std::uint64_t *getDefinedData() { return m_defined.data(); }
        const std::uint64_t *getDefinedData() const { return m_defined.data(); }
        
        bool anyUndefined(size_t offset, size_t size);
    protected:
        size_t m_size;
        std::vector<std::uint64_t> m_values;
        std::vector<std::uint64_t> m_defined;
};

struct StateMapping
{
    std::map<hlim::NodePort, size_t> outputToOffset;
    std::map<std::string, size_t> clockToSignalIdx;
    std::map<std::string, size_t> clockToClkIdx;
    std::map<std::string, size_t> resetToSignalIdx;
    
    StateMapping() { clear(); }
    
    void clear() { outputToOffset.clear(); outputToOffset[{}] = ~0ull; clockToSignalIdx.clear(); clockToClkIdx.clear(); resetToSignalIdx.clear(); }    
};

class ExecutionBlock
{
    public:
        void execute(DataState &state) const;

        void addInstruction(const std::function<void(DataState &state)> &inst);
    protected:
        std::vector<size_t> m_dependsOnExecutionBlocks;
        std::vector<size_t> m_dependentExecutionBlocks;
        std::vector<std::function<void(DataState &state)>> m_instructions;
};

class HardwareAssert
{
    public:
    protected:
};

class Register
{
    public:
        Register(unsigned size, unsigned srcData, unsigned srcReset, unsigned dst);
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
    size_t simulationTick = 0;
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
        void compileProgram(const hlim::Circuit &circuit);

        void executeCombinatory(ExecutionState &executionState, DataState &dataState, bool forceFullReevaluation = false) const;
        void advanceRegisters(ExecutionState &executionState, DataState &dataState) const;
        
        
        inline const DataState &getInitialState() const { return m_initialState; }
        inline size_t getFullStateWidth() const { return m_fullStateWidth; }
        inline const StateMapping &getStateMapping() const { return m_stateMapping; }
        inline const std::vector<ExecutionBlock> &getExecutionBlocks() const { return m_executionBlocks; }
    protected:
        DataState m_initialState;
        size_t m_fullStateWidth;
        
        StateMapping m_stateMapping;

        std::map<size_t, std::vector<size_t>> m_clockIdx2clockDomain;
        std::vector<ClockDriver> m_clockDrivers;
        std::vector<ClockDomain> m_clockDomains;
        std::vector<ExecutionBlock> m_executionBlocks;
        
        void allocateSignals(const hlim::Circuit &circuit);
};
    
class ReferenceSimulator : public Simulator
{
    public:
        virtual void compileProgram(const hlim::Circuit &circuit) override;
        
        virtual void reset() override;
        
        virtual void reevaluate() override;
        virtual void advanceAnyTick() override;
        
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) override;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::BaseClock *clk) override;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) override;
    protected:
        Program m_program;
        ExecutionState m_executionState;
        DataState m_dataState;
};

}
