#pragma once

#include "../../simulation/SimulatorCallbacks.h"

#include <fstream>
#include <map>
#include <sstream>

namespace hcl::core::sim {
    class Simulator;
}

namespace hcl::core::vhdl {

class VHDLExport;
class AST;

class TestbenchRecorder : public sim::SimulatorCallbacks
{
    public:
        TestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, const std::string &filename);
        ~TestbenchRecorder();

        virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
        /*
        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
        */
        virtual void onSimProcOutputOverridden(hlim::NodePort output, const sim::DefaultBitVectorState &state) override;
        virtual void onSimProcOutputRead(hlim::NodePort output, const sim::DefaultBitVectorState &state) override;

    protected:
        VHDLExport &m_exporter;
        AST *m_ast;
        sim::Simulator &m_simulator;
        std::fstream m_testbenchFile;
        hlim::ClockRational m_lastSimulationTime;

        std::map<hlim::NodePort, std::string> m_outputToIoPinName;

        std::stringstream m_assertStatements;

        void writeHeader();
        void writeFooter();
};


}