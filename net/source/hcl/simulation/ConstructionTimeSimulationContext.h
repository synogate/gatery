#pragma once

#include "SimulationContext.h"

#include <map>


namespace hcl::core::sim {

class ConstructionTimeSimulationContext : public SimulationContext {
    public:
        virtual void overrideSignal(hlim::NodePort output, const DefaultBitVectorState &state) override;
        virtual void getSignal(hlim::NodePort output, DefaultBitVectorState &state) override;

        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor) override;
        virtual void simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil) override;
    protected:
        std::map<hlim::NodePort, DefaultBitVectorState> m_overrides;
};

}