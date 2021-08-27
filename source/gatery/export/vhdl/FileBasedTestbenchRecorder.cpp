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
#include "FileBasedTestbenchRecorder.h"

#include "AST.h"
#include "Entity.h"

#include "../../simulation/Simulator.h"

#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_Signal.h"


#include <sstream>
#include <iostream>

namespace gtry::vhdl {


FileBasedTestbenchRecorder::FileBasedTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, std::string name) : BaseTestbenchRecorder(ast, simulator, std::move(name)), m_exporter(exporter)
{
    m_dependencySortedEntities.push_back(m_name);
    m_testVectorFilename = m_name + ".testvectors";
    m_auxiliaryDataFiles.push_back(m_testVectorFilename);
    m_testbenchFile.open((basePath / m_testVectorFilename).string().c_str(), std::fstream::out);

    m_vhdlFilename = m_ast->getFilename(basePath, m_name);
}

FileBasedTestbenchRecorder::~FileBasedTestbenchRecorder()
{
}

void FileBasedTestbenchRecorder::writeVHDL(std::filesystem::path path, const std::string &testVectorFilename)
{
    std::fstream vhdlFile(path.string().c_str(), std::fstream::out);

    vhdlFile << R"(
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;
USE std.textio.all;

ENTITY )" << m_dependencySortedEntities.back() << R"( IS
END )" << m_dependencySortedEntities.back() << R"(;

ARCHITECTURE tb OF )" << m_dependencySortedEntities.back() << R"( IS

)";

    auto *rootEntity = m_ast->getRootEntity();

    const auto &allClocks = rootEntity->getClocks();
    const auto &allResets = rootEntity->getResets();
    const auto &allIOPins = rootEntity->getIoPins();

    m_clocksOfInterest.insert(allClocks.begin(), allClocks.end());
    m_resetsOfInterest.insert(allResets.begin(), allResets.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();

    declareSignals(vhdlFile, allClocks, allResets, allIOPins);

    std::map<hlim::NodePort, bool> outputIsBool;    
    std::set<hlim::NodePort> outputIsDrivenByNetwork;

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);
        auto conType = ioPin->getConnectionType();

        if (ioPin->isOutputPin()) {
            m_outputToIoPinName[ioPin->getDriver(0)] = name;
            outputIsBool[ioPin->getDriver(0)] = conType.interpretation == hlim::ConnectionType::BOOL;
            outputIsDrivenByNetwork.insert(ioPin->getDriver(0));
        }

        if (ioPin->isInputPin()) {
            m_outputToIoPinName[{.node=ioPin, .port=0}] = name;
            outputIsBool[{.node=ioPin, .port=0}] = conType.interpretation == hlim::ConnectionType::BOOL;
        }

    }

    vhdlFile << R"(
    function stringcompare(v_line : in string; str : in string) return boolean is
    begin
        if not (v_line'length = str'length) then
            return false;
        end if;
        for i in v_line'range loop
            if not (v_line(i) = str(i)) then
                return false;
            end if;
        end loop;
        return true;
     
    end function stringcompare;
    )";
    

    vhdlFile << "BEGIN" << std::endl;

    cf.indent(vhdlFile, 1);
    vhdlFile << "inst_root : entity work." << rootEntity->getName() << "(impl) port map (" << std::endl;

    writePortmap(m_testbenchFile, allClocks, allResets, allIOPins);

    cf.indent(vhdlFile, 1);
    vhdlFile << ");" << std::endl;

    cf.indent(vhdlFile, 1);
    vhdlFile << "sim_process : PROCESS" << std::endl;

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);

        hlim::ConnectionType conType = ioPin->getConnectionType();

        vhdlFile << "    VARIABLE v_" << name << " : ";
        cf.formatConnectionType(vhdlFile, conType);
        vhdlFile << ';' << std::endl;
    }

    cf.indent(vhdlFile, 2);
    vhdlFile << "VARIABLE v_line : line;" << std::endl;
    cf.indent(vhdlFile, 2);
    vhdlFile << "VARIABLE time_in_ps : integer;" << std::endl;
    cf.indent(vhdlFile, 2);
    vhdlFile << "VARIABLE v_clk : std_logic;" << std::endl;

    cf.indent(vhdlFile, 2);
    vhdlFile << "FILE test_vector_file : text;" << std::endl;


    cf.indent(vhdlFile, 1);
    vhdlFile << "BEGIN" << std::endl;

    initClocks(vhdlFile, allClocks, allResets);

    cf.indent(vhdlFile, 2);
    vhdlFile << "file_open(test_vector_file, \"" << testVectorFilename << "\", read_mode);" << std::endl;

    cf.indent(vhdlFile, 2);
    vhdlFile << "IF endfile(test_vector_file) THEN" << std::endl;
    cf.indent(vhdlFile, 3);
    vhdlFile << "REPORT \"The test vector file is empty!\";" << std::endl;
    cf.indent(vhdlFile, 3);
    vhdlFile << "ASSERT FALSE;" << std::endl;
    cf.indent(vhdlFile, 2);
    vhdlFile << "END IF;" << std::endl;

    cf.indent(vhdlFile, 2);
    vhdlFile << "WHILE NOT endfile(test_vector_file) loop" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "readline(test_vector_file, v_line);" << std::endl;
    
    cf.indent(vhdlFile, 3);
    vhdlFile << "IF stringcompare(v_line(1 to v_line'length), \"SET\") THEN" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "IF false THEN" << std::endl;
        for (auto &p : m_outputToIoPinName) {
            if (outputIsDrivenByNetwork.contains(p.first)) continue;

            cf.indent(vhdlFile, 4);
            vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"" << p.second << "\") THEN" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

            cf.indent(vhdlFile, 5);
            if (outputIsBool[p.first])
                vhdlFile << "read(v_line, v_" << p.second << ");" << std::endl;
            else
                vhdlFile << "bread(v_line, v_" << p.second << ");" << std::endl;
            cf.indent(vhdlFile, 5);
            vhdlFile << p.second << " <= v_" << p.second << ";" << std::endl;
        }
        cf.indent(vhdlFile, 4);
        vhdlFile << "ELSE" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "REPORT \"An error occured while parsing the test vector file: unknown signal:\" & v_line(1 to v_line'length);" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "END IF;" << std::endl;


    cf.indent(vhdlFile, 3);
    vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"CHECK\") THEN" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "IF false THEN" << std::endl;
        for (auto &p : m_outputToIoPinName) {

            cf.indent(vhdlFile, 4);
            vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"" << p.second << "\") THEN" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

            cf.indent(vhdlFile, 5);
            if (outputIsBool[p.first])
                vhdlFile << "read(v_line, v_" << p.second << ");" << std::endl;
            else
                vhdlFile << "bread(v_line, v_" << p.second << ");" << std::endl;
            cf.indent(vhdlFile, 5);
            vhdlFile << "ASSERT std_match(" << p.second << ", v_" << p.second << ") severity failure;" << std::endl;
        }
        cf.indent(vhdlFile, 4);
        vhdlFile << "ELSE" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "REPORT \"An error occured while parsing the test vector file: unknown signal:\" & v_line(1 to v_line'length);" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "END IF;" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"CLK\") THEN" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "IF false THEN" << std::endl;
        for (auto c : m_clocksOfInterest) {

            auto *rootEntity = m_ast->getRootEntity();
            std::string clockName =  rootEntity->getNamespaceScope().getName((hlim::Clock *) c);


            cf.indent(vhdlFile, 4);
            vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"" << clockName << "\") THEN" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "read(v_line, v_clk);" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << clockName << " <= v_clk;" << std::endl;
        }
        cf.indent(vhdlFile, 4);
        vhdlFile << "ELSE" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "REPORT \"An error occured while parsing the test vector file: unknown clock:\" & v_line(1 to v_line'length);" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "END IF;" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"RST\") THEN" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "IF false THEN" << std::endl;
        for (auto c : m_resetsOfInterest) {

            auto *rootEntity = m_ast->getRootEntity();
            std::string resetName =  rootEntity->getNamespaceScope().getResetName((hlim::Clock *) c);


            cf.indent(vhdlFile, 4);
            vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"" << resetName << "\") THEN" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << "read(v_line, v_clk);" << std::endl;

            cf.indent(vhdlFile, 5);
            vhdlFile << resetName << " <= v_clk;" << std::endl;
        }
        cf.indent(vhdlFile, 4);
        vhdlFile << "ELSE" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "REPORT \"An error occured while parsing the test vector file: unknown clock:\" & v_line(1 to v_line'length);" << std::endl;
        cf.indent(vhdlFile, 5);
        vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "END IF;" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"ADV\") THEN" << std::endl;

        cf.indent(vhdlFile, 4);
        vhdlFile << "readline(test_vector_file, v_line);" << std::endl;
        cf.indent(vhdlFile, 4);
        vhdlFile << "read(v_line, time_in_ps);" << std::endl;
        cf.indent(vhdlFile, 4);
        vhdlFile << "wait for time_in_ps * 1 ps;" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "ELSE" << std::endl;

    cf.indent(vhdlFile, 4);
    vhdlFile << "REPORT \"An error occured while parsing the test vector file: Can't parse line:\" & v_line(1 to v_line'length);" << std::endl;
    cf.indent(vhdlFile, 4);
    vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

    cf.indent(vhdlFile, 3);
    vhdlFile << "END IF;" << std::endl;

    cf.indent(vhdlFile, 2);
    vhdlFile << "end loop;" << std::endl;


    vhdlFile << "WAIT;" << std::endl;
    vhdlFile << "END PROCESS;" << std::endl;
    vhdlFile << "END;" << std::endl;

    m_lastSimulationTime = 0;
}



namespace {
    void formatTime(std::ostream &stream, hlim::ClockRational time) {
        std::string unit = "sec";
        if (time.denominator() > 1) { unit = "ms"; time *= 1000; }
        if (time.denominator() > 1) { unit = "us"; time *= 1000; }
        if (time.denominator() > 1) { unit = "ns"; time *= 1000; }
        if (time.denominator() > 1) { unit = "ps"; time *= 1000; }
        if (time.denominator() > 1) { unit = "fs"; time *= 1000; }

        stream << time.numerator() / time.denominator() << ' ' << unit;
    }
}

void FileBasedTestbenchRecorder::onPowerOn()
{
    writeVHDL(m_vhdlFilename, m_testVectorFilename);
}


void FileBasedTestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();

    for (const auto &p : m_signalOverrides) 
        m_testbenchFile << "SET\n" << p.first << '\n' << p.second << '\n';

    m_signalOverrides.clear();

    auto timeDiff = simulationTime - m_lastSimulationTime;
    m_lastSimulationTime = simulationTime;

    auto timeDiffInPS = timeDiff * 1'000'000'000'000ull;
    auto roundedTimeDiffInPS = timeDiffInPS.numerator() / timeDiffInPS.denominator();

    // All asserts are collected to be triggered halfway between the last tick (when signals were set) and the next tick (when new stuff happens).
    if (m_assertStatements.str().empty()) {
        m_testbenchFile << "ADV\n" << roundedTimeDiffInPS << std::endl;
    } else {
        m_testbenchFile << "ADV\n" << roundedTimeDiffInPS/2 << std::endl;

        m_testbenchFile << m_assertStatements.str();
        m_assertStatements.str(std::string());

        m_testbenchFile << "ADV\n" << roundedTimeDiffInPS/2 << std::endl;
    }
}

void FileBasedTestbenchRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
    if (!m_clocksOfInterest.contains(clock)) return;

    auto *rootEntity = m_ast->getRootEntity();

    m_testbenchFile << "CLK\n" << rootEntity->getNamespaceScope().getName((hlim::Clock *) clock) << std::endl;

    if (risingEdge)
        m_testbenchFile << "1\n";
    else
        m_testbenchFile << "0\n";
}

void FileBasedTestbenchRecorder::onReset(const hlim::Clock *clock, bool resetAsserted)
{
    if (!m_resetsOfInterest.contains(clock)) return;

    auto *rootEntity = m_ast->getRootEntity();

    m_testbenchFile << "RST\n" << rootEntity->getNamespaceScope().getResetName((hlim::Clock *) clock) << std::endl;

    if (resetAsserted)
        m_testbenchFile << "1\n";
    else
        m_testbenchFile << "0\n";
}

void FileBasedTestbenchRecorder::onSimProcOutputOverridden(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT(name_it != m_outputToIoPinName.end());

    std::stringstream str_state;
    str_state << state;

    m_signalOverrides[name_it->second] = str_state.str();
}

void FileBasedTestbenchRecorder::onSimProcOutputRead(hlim::NodePort output, const sim::DefaultBitVectorState &state)
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
    if (name_it == m_outputToIoPinName.end()) {
        if (dynamic_cast<hlim::Node_Pin*>(output.node)) return;

        if (auto *signal = dynamic_cast<hlim::Node_Signal*>(output.node))
            if (dynamic_cast<hlim::Node_Pin*>(signal->getNonSignalDriver(0).node))
                return;

        HCL_ASSERT_HINT(false, "Can only record asserts for signals that are output pins!");
    }

    const auto& conType = hlim::getOutputConnectionType(output);
    if (conType.interpretation == hlim::ConnectionType::BOOL) {
        if (state.get(sim::DefaultConfig::DEFINED, 0)) {
            m_assertStatements << "CHECK" << std::endl << name_it->second << std::endl << state << std::endl;
        }
    } else {
        bool allDefined = true;
        bool anyDefined = false;
        for (auto i : utils::Range(conType.width)) {
            bool d = state.get(sim::DefaultConfig::DEFINED, i);
            allDefined &= d;
            anyDefined |= d;
        }

        if (anyDefined) {
            m_assertStatements << "CHECK" << std::endl << name_it->second << std::endl;
            for (int i = (int)conType.width - 1; i >= 0; i--) {
                bool d = state.get(sim::DefaultConfig::DEFINED, i);
                bool v = state.get(sim::DefaultConfig::VALUE, i);
                if (d)
                    m_assertStatements << (v?'1':'0');
                else
                    m_assertStatements << '-';
            }
            m_assertStatements << std::endl;
        }
    }
}


}
