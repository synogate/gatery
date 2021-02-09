#pragma once

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

namespace hcl::core::sim {

class SimulationContext {
    public:
        SimulationContext();
        virtual ~SimulationContext();

        virtual void overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state) = 0;
        virtual void getSignal(hlim::NodePort output, DefaultBitVectorState &state) = 0;

        static SimulationContext *current() { return m_current; }
    protected:
        thread_local static SimulationContext *m_current;
};

}