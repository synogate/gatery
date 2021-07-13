/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "../../simulation/SimulatorCallbacks.h"

#include <filesystem>

#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace gtry::sim {
    class Simulator;
}

namespace gtry::vhdl {

class VHDLExport;
class AST;

class FileBasedTestbenchRecorder : public sim::SimulatorCallbacks
{
    public:
        FileBasedTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, const std::string &name);
        ~FileBasedTestbenchRecorder();

        const std::string &getName() const { return m_name; }

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
        std::string m_name;
        std::fstream m_testbenchFile;
        hlim::ClockRational m_lastSimulationTime;

        std::set<const hlim::Clock*> m_clocksOfInterest;

        std::map<hlim::NodePort, std::string> m_outputToIoPinName;

        std::stringstream m_assertStatements;

        void writeVHDL(std::filesystem::path path, const std::string &testVectorFilename);
};


}
