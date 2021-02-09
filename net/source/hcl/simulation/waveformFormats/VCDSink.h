#pragma once

#include "../WaveformRecorder.h"

#include <fstream>
#include <string>
#include <map>

namespace hcl::core::sim {

class VCDSink : public WaveformRecorder
{
    public:
        VCDSink(hlim::Circuit &circuit, Simulator &simulator, const char *filename, const char *logFilename = nullptr);

        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
    protected:
        std::fstream m_vcdFile;
        std::fstream m_logFile;
        bool m_doWriteLogFile;

        std::vector<std::string> m_id2sigCode;
        std::map<hlim::Clock*, std::string> m_clock2code;
        std::vector<hlim::Clock*> m_allClocks;

        virtual void initialize() override;
        virtual void signalChanged(size_t id) override;
        virtual void advanceTick(const hlim::ClockRational &simulationTime) override;

        void stateToFile(size_t offset, size_t size);
};

}