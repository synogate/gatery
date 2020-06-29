#pragma once

#include "../hlim/NodeIO.h"
#include "../hlim/Clock.h"
#include "BitVectorState.h"
#include "../utils/BitManipulation.h"

#include <vector>
#include <functional>
#include <map>
#include <set>

namespace hcl::core::sim {

class Simulator
{
    public:
        virtual void compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs = {}) = 0;
        
        virtual void reset() = 0;
        
        virtual void reevaluate() = 0;
        virtual void advanceAnyTick() = 0;
        
        virtual DefaultBitVectorState getValueOfInternalState(const hlim::BaseNode *node, size_t idx) = 0;
        virtual DefaultBitVectorState getValueOfOutput(const hlim::NodePort &nodePort) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfClock(const hlim::BaseClock *clk) = 0;
        virtual std::array<bool, DefaultConfig::NUM_PLANES> getValueOfReset(const std::string &reset) = 0;
    
        inline const hlim::ClockRational &getCurrentimulationTime() { return m_simulationTime; }
    protected:
        hlim::ClockRational m_simulationTime;
};

}
