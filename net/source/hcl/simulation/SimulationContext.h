#pragma once

#include "../hlim/NodePort.h"
#include "BitVectorState.h"

#include <coroutine>

namespace hcl::core::sim {


class WaitFor;
class WaitUntil;

class SimulationContext {
    public:
        SimulationContext();
        virtual ~SimulationContext();

        virtual void overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state) = 0;
        virtual void getSignal(hlim::NodePort output, DefaultBitVectorState &state) = 0;

        virtual void simulationFiberSuspending(std::coroutine_handle<> handle, WaitFor &waitFor) = 0;
        virtual void simulationFiberSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil) = 0;

        static SimulationContext *current() { return m_current; }
    protected:
        SimulationContext *m_overshadowed = nullptr;
        thread_local static SimulationContext *m_current;
};

}