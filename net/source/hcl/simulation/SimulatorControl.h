#pragma once

#include "Simulator.h"
#include "BitVectorState.h"

#include "../hlim/NodeIO.h"

#include <functional>
#include <vector>
#include <string>

namespace mhdl::core::sim {

class SimulatorControl
{
    public:
        struct StateBreakpoint {
            enum Trigger {
                TRIG_CHANGE,
                TRIG_EQUAL,
                TRIG_NOT_EQUAL
            };
            Trigger trigger;
            DefaultBitVectorState refValue;
            
            size_t stateOffset;
        };
        
        struct SignalBreakpoint : StateBreakpoint {
            hlim::NodePort nodePort;
        };
        
        void bindSimulator(Simulator *simulator);
        
        

        void advanceAnyTick();
        void advanceTick(const std::string &clk);
        void freeRun(const std::function<bool()> &tickCallback, std::vector<size_t> &triggeredBreakpoints);
    protected:
        Simulator *m_simulator = nullptr;
        std::vector<SignalBreakpoint> m_signalBreakpoints;
};

}
