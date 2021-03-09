#pragma once

#include "../WaveformRecorder.h"

#include "MemoryTrace.h"
#include "../BitAllocator.h"

#include <fstream>
#include <string>
#include <map>

namespace hcl::core::sim {

class MemoryTraceRecorder : public WaveformRecorder
{
    public:
        MemoryTraceRecorder(hlim::Circuit &circuit, Simulator &simulator, bool startImmediately = true);

        void start();
        void stop();

        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;

        inline const MemoryTrace &getTrace() const { return m_trace; }
    protected:
        bool m_record;

        BitAllocator m_bitAllocator;
        MemoryTrace m_trace;

        std::map<const hlim::Clock*, unsigned> m_clock2idx;

        virtual void initialize() override;
        virtual void signalChanged(size_t id) override;
        virtual void advanceTick(const hlim::ClockRational &simulationTime) override;
};

}