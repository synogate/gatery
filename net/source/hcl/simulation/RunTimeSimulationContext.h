#pragma once

#include "SimulationContext.h"

namespace hcl::core::sim {

class Simulator;

class RunTimeSimulationContext : public SimulationContext {
    public:
        RunTimeSimulationContext(Simulator *simulator);

        virtual void overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state) override;
        virtual void getSignal(hlim::NodePort output, DefaultBitVectorState &state) override;

        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor) override;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil) override;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock) override;
    protected:
        Simulator *m_simulator;
};

}