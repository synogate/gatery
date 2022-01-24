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
#include "gatery/pch.h"
#include "TestbenchRecorder.h"

#include "AST.h"
#include "Entity.h"

#include "../../simulation/Simulator.h"

#include "../../hlim/coreNodes/Node_Pin.h"


#include <sstream>
#include <iostream>

namespace gtry::vhdl {


TestbenchRecorder::TestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, std::string name) : BaseTestbenchRecorder(ast, simulator, std::move(name)), m_exporter(exporter)
{
    m_dependencySortedEntities.push_back(m_name);
    m_testbenchFile.open(m_ast->getFilename(basePath, m_name).c_str(), std::fstream::out);
}

TestbenchRecorder::~TestbenchRecorder()
{
    writeFooter();
}

void TestbenchRecorder::writeHeader()
{
    m_testbenchFile << R"(
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;
use std.env.finish;

ENTITY )" << m_name << R"( IS
END )" << m_name << R"(;

ARCHITECTURE tb OF )" << m_name << R"( IS

)";

    auto *rootEntity = m_ast->getRootEntity();

    const auto &allClocks = rootEntity->getClocks();
    const auto &allResets = rootEntity->getResets();
    const auto &allIOPins = rootEntity->getIoPins();

    m_clocksOfInterest.insert(allClocks.begin(), allClocks.end());
    m_resetsOfInterest.insert(allResets.begin(), allResets.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();

    declareSignals(m_testbenchFile, allClocks, allResets, allIOPins);

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().get(ioPin).name;

        if (ioPin->isOutputPin())
            m_outputToIoPinName[ioPin->getDriver(0)] = name;

        if (ioPin->isInputPin())
            m_outputToIoPinName[{.node=ioPin, .port=0}] = name;
    }

    m_testbenchFile << "BEGIN" << std::endl;

    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "inst_root : entity work." << rootEntity->getName() << "(impl) port map (" << std::endl;

    writePortmap(m_testbenchFile, allClocks, allResets, allIOPins);
   
    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << ");" << std::endl;

    for (auto &clock : allClocks)
        buildClockProcess(m_testbenchFile, clock);

    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "sim_process : PROCESS" << std::endl;
    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "BEGIN" << std::endl;


    m_vhdlSimulationTime = 0;
    m_currentSimulationTime = 0;
}

void TestbenchRecorder::writeFooter()
{
//    CodeFormatting &cf = m_ast->getCodeFormatting();
//    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "TB_testbench_is_done <= '1';" << std::endl;
    m_testbenchFile << "WAIT;" << std::endl;
    m_testbenchFile << "END PROCESS;" << std::endl;
    m_testbenchFile << "END;" << std::endl;
}





void TestbenchRecorder::onPowerOn()
{
    writeHeader();
}

void TestbenchRecorder::advanceTimeTo(hlim::ClockRational simulationTime)
{
    auto deltaT = simulationTime - m_vhdlSimulationTime;

    const std::array<std::string, 6> unit2str = {"sec", "ms", "us", "ns", "ps", "fs"};
    const std::array<uint64_t, 6> unit2denom = {1ull, 1'000ull, 1'000'000ull, 1'000'000'000ull, 1'000'000'000'000ull, 1'000'000'000'000'000ull};

    size_t unit = 0;
    while (deltaT.denominator() > 1 && unit+1 < unit2str.size()) { 
        unit++; 
        deltaT *= 1000; 
    }

    CodeFormatting &cf = m_ast->getCodeFormatting();

    size_t t = deltaT.numerator() / deltaT.denominator();
    if (t > 1'000'000 && unit > 1) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << "WAIT FOR " << (t / 1'000'000) << ' ' << unit2str[unit-2] << ";\n";
        m_vhdlSimulationTime += hlim::ClockRational(t / 1'000'000, unit2denom[unit-2]);

        t = t % 1'000'000;
    }

    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "WAIT FOR " << t << ' ' << unit2str[unit] << ";\n";
    m_vhdlSimulationTime += hlim::ClockRational(t, unit2denom[unit]);
}

void TestbenchRecorder::commitTime()
{
    advanceTimeTo(m_currentSimulationTime);
}

void TestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
    if (m_assertStatements.str().empty() && m_signalOverrides.empty() && m_resetOverrides.empty()) {
        m_currentSimulationTime = simulationTime;
        return;
    }

    CodeFormatting &cf = m_ast->getCodeFormatting();

    auto timeDiff = simulationTime - m_currentSimulationTime;

    auto updateTime = m_currentSimulationTime + timeDiff * hlim::ClockRational(1,2);
    auto checkTime = m_currentSimulationTime + timeDiff * hlim::ClockRational(3,4);
    m_currentSimulationTime = simulationTime;


    // Split stuff up in between the time step to allow the vhdl simulators to correctly schedule their processes.
    // Set signals first, check asserts after.
    
    if (!m_signalOverrides.empty() || !m_resetOverrides.empty()) {
        advanceTimeTo(updateTime);

        for (const auto &p : m_signalOverrides)
            m_testbenchFile << p.second;

        for (const auto &p : m_resetOverrides)
            m_testbenchFile << p.second;
    
        m_signalOverrides.clear();
        m_resetOverrides.clear();    
    }

    if (!m_assertStatements.str().empty()) {
        advanceTimeTo(checkTime);
        m_testbenchFile << m_assertStatements.str();

        m_assertStatements.str(std::string());
    }
}

void TestbenchRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
    // handled by separate clock process
/*
    if (!m_clocksOfInterest.contains(clock)) return;

    CodeFormatting &cf = m_ast->getCodeFormatting();
    auto *rootEntity = m_ast->getRootEntity();

    cf.indent(m_testbenchFile, 2);
    if (risingEdge)
        m_testbenchFile << rootEntity->getNamespaceScope().getClock((hlim::Clock *) clock).name << " <= '1';" << std::endl;
    else
        m_testbenchFile << rootEntity->getNamespaceScope().getClock((hlim::Clock *) clock).name << " <= '0';" << std::endl;
*/
}

void TestbenchRecorder::onReset(const hlim::Clock *clock, bool resetAsserted)
{
    if (!m_resetsOfInterest.contains(clock)) return;

    CodeFormatting &cf = m_ast->getCodeFormatting();
    auto *rootEntity = m_ast->getRootEntity();

    const std::string rst = rootEntity->getNamespaceScope().getReset((hlim::Clock *) clock).name;

    std::stringstream assignment;
    cf.indent(assignment, 2);
    assignment << rst << " <= " << (resetAsserted?"'1'":"'0'") << ";\n";

    m_resetOverrides[rst] = assignment.str();
}

void TestbenchRecorder::onSimProcOutputOverridden(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT(name_it != m_outputToIoPinName.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();
    auto *rootEntity = m_ast->getRootEntity();

    std::stringstream str_state;

    cf.indent(str_state, 2);
    str_state << name_it->second << " <= ";

    const auto& conType = hlim::getOutputConnectionType(output);

    char sep = '"';
    if (conType.interpretation == hlim::ConnectionType::BOOL)
        sep = '\'';

    str_state << sep;
    str_state << state;
    str_state << sep << ';' << std::endl;


    m_signalOverrides[name_it->second] = str_state.str();
}

void TestbenchRecorder::onSimProcOutputRead(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    // find output driving output pin
    for (auto nh : output.node->exploreOutput(output.port)) {
        if (auto* res = dynamic_cast<hlim::Node_Pin*>(nh.node())) {
            output = nh.node()->getDriver(0);
        } else
            if (!nh.isSignal())
                nh.backtrack();
    }    

    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT_HINT(name_it != m_outputToIoPinName.end(), "Can only record asserts for signals that are output pins!");

    CodeFormatting &cf = m_ast->getCodeFormatting();

    const auto& conType = hlim::getOutputConnectionType(output);
    if (conType.interpretation == hlim::ConnectionType::BOOL) {
        if (state.get(sim::DefaultConfig::DEFINED, 0)) {
            cf.indent(m_assertStatements, 2);
            m_assertStatements << "ASSERT " << name_it->second << " = '" << state << "';" << std::endl;
        }
    } else {
        // If all bits are defined, assert on the entire vector, otherwise build individual asserts for each valid bit.
        bool allDefined = true;
        for (auto i : utils::Range(conType.width))
            allDefined &= state.get(sim::DefaultConfig::DEFINED, i);

        if (allDefined) {
            cf.indent(m_assertStatements, 2);
            m_assertStatements << "ASSERT " << name_it->second << " = \"" << state << "\";" << std::endl;
        } else {
            for (auto i : utils::Range(conType.width))
                if (state.get(sim::DefaultConfig::DEFINED, i)) {
                    char v = state.get(sim::DefaultConfig::VALUE, i)?'1':'0';
                    m_assertStatements << "ASSERT " << name_it->second << '('<<i<<") = '" << v << "';" << std::endl;
                }
        }
    }
}

void TestbenchRecorder::onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();

    commitTime();

    m_testbenchFile << std::endl;
    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "-- Begin: " << id << std::endl;
    if (!desc.empty()) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << "-- ";
        for (auto i : utils::Range(desc.size())) {
            m_testbenchFile << desc[i];
            if (desc[i] == '\n' && i+1 < desc.size()) {
                cf.indent(m_testbenchFile, 2);
                m_testbenchFile << "-- ";
            }
        }
        m_testbenchFile << std::endl;
    }
}

void TestbenchRecorder::onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();

    commitTime();

    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "-- End: " << id << std::endl;
    m_testbenchFile << std::endl;
}




}
