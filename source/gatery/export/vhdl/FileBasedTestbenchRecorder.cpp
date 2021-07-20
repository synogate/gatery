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


FileBasedTestbenchRecorder::FileBasedTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, std::string name) : BaseTestbenchRecorder(std::move(name)), m_exporter(exporter), m_ast(ast), m_simulator(simulator)
{
    m_dependencySortedEntities.push_back(m_name);
    std::string testVectorFilename = m_name + ".testvectors";
    m_auxiliaryDataFiles.push_back(testVectorFilename);
    m_testbenchFile.open((basePath / testVectorFilename).string().c_str(), std::fstream::out);

    writeVHDL(m_ast->getFilename(basePath, m_name), testVectorFilename);

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
    const auto &allIOPins = rootEntity->getIoPins();

    m_clocksOfInterest.insert(allClocks.begin(), allClocks.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();

    for (auto clock : allClocks) {
        vhdlFile << "    SIGNAL " << rootEntity->getNamespaceScope().getName(clock) << " : STD_LOGIC;" << std::endl;
        if (clock->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
            vhdlFile << "    SIGNAL " << rootEntity->getNamespaceScope().getName(clock)<<clock->getResetName() << " : STD_LOGIC;" << std::endl;
    }

    std::map<hlim::NodePort, bool> outputIsBool;    
    std::set<hlim::NodePort> outputIsDrivenByNetwork;

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);
        bool isInput = !ioPin->getDirectlyDriven(0).empty();
        bool isOutput = ioPin->getNonSignalDriver(0).node != nullptr;

        hlim::ConnectionType conType;
        if (isOutput) {
            auto driver = ioPin->getNonSignalDriver(0);
            conType = hlim::getOutputConnectionType(driver);
        } else
            conType = ioPin->getOutputConnectionType(0);

        vhdlFile << "    SIGNAL " << name << " : ";
        cf.formatConnectionType(vhdlFile, conType);
        vhdlFile << ';' << std::endl;

        

        if (isOutput) {
            m_outputToIoPinName[ioPin->getDriver(0)] = name;
            outputIsBool[ioPin->getDriver(0)] = conType.interpretation == hlim::ConnectionType::BOOL;
            outputIsDrivenByNetwork.insert(ioPin->getDriver(0));
        }

        if (isInput) {
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

    std::vector<std::string> portmapList;

    for (auto &s : allClocks) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getName(s) << " => ";
        line << rootEntity->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
        if (s->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
            std::stringstream line;
            line << rootEntity->getNamespaceScope().getName(s)<<s->getResetName() << " => ";
            line << rootEntity->getNamespaceScope().getName(s)<<s->getResetName();
            portmapList.push_back(line.str());
        }
    }
    for (auto &s : allIOPins) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getName(s) << " => ";
        line << rootEntity->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }

    for (auto i : utils::Range(portmapList.size())) {
        cf.indent(vhdlFile, 2);
        vhdlFile << portmapList[i];
        if (i+1 < portmapList.size())
            vhdlFile << ",";
        vhdlFile << std::endl;
    }


    cf.indent(vhdlFile, 1);
    vhdlFile << ");" << std::endl;

    cf.indent(vhdlFile, 1);
    vhdlFile << "sim_process : PROCESS" << std::endl;

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);
        bool isOutput = ioPin->getNonSignalDriver(0).node != nullptr;

        hlim::ConnectionType conType;
        if (isOutput) {
            auto driver = ioPin->getNonSignalDriver(0);
            conType = hlim::getOutputConnectionType(driver);
        } else
            conType = ioPin->getOutputConnectionType(0);

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


    for (auto &s : allClocks) {
        cf.indent(vhdlFile, 2);
        vhdlFile << rootEntity->getNamespaceScope().getName(s) << " <= '0';" << std::endl;
        if (s->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
            cf.indent(vhdlFile, 2);
            vhdlFile << rootEntity->getNamespaceScope().getName(s)<<s->getResetName() << " <= '1';" << std::endl;
        }
    }

    cf.indent(vhdlFile, 2);
    vhdlFile << "WAIT FOR 1 ns;" << std::endl;
    for (auto &s : allClocks) {
        cf.indent(vhdlFile, 2);
        vhdlFile << rootEntity->getNamespaceScope().getName(s) << " <= '1';" << std::endl;
    }
    cf.indent(vhdlFile, 2);
    vhdlFile << "WAIT FOR 1 ns;" << std::endl;

    for (auto &s : allClocks) {
        cf.indent(vhdlFile, 2);
        vhdlFile << rootEntity->getNamespaceScope().getName(s) << " <= '0';" << std::endl;
        if (s->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
            cf.indent(vhdlFile, 2);
            vhdlFile << rootEntity->getNamespaceScope().getName(s)<<s->getResetName() << " <= '0';" << std::endl;
        }
    }
    cf.indent(vhdlFile, 2);
    vhdlFile << "WAIT FOR 1 ns;" << std::endl;

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
            vhdlFile << "ASSERT " << p.second << " = v_" << p.second << " severity failure;" << std::endl;
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

        if (time.denominator() > 1) std::cout << "Warning, rounding fractional to nearest integer femtosecond" << std::endl;

        stream << time.numerator() / time.denominator() << ' ' << unit;
    }
}

void FileBasedTestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();

    auto timeDiff = simulationTime - m_lastSimulationTime;
    m_lastSimulationTime = simulationTime;

    auto timeDiffInPS = timeDiff * 1'000'000'000'000ull;
    auto roundedTimeDiffInPS = timeDiffInPS.numerator() / timeDiffInPS.denominator();

    // All asserts are collected to be triggered halfway between the last tick (when signals were set) and the next tick (when new stuff happens).
    if (m_assertStatements.str().empty()) {
        m_testbenchFile << "ADV" << std::endl << roundedTimeDiffInPS << std::endl;
    } else {
        m_testbenchFile << "ADV" << std::endl << roundedTimeDiffInPS/2 << std::endl;

        m_testbenchFile << m_assertStatements.str();
        m_assertStatements.str(std::string());

        m_testbenchFile << "ADV" << std::endl << roundedTimeDiffInPS/2 << std::endl;
    }
}

void FileBasedTestbenchRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
    if (!m_clocksOfInterest.contains(clock)) return;

    auto *rootEntity = m_ast->getRootEntity();

    m_testbenchFile << "CLK" << std::endl << rootEntity->getNamespaceScope().getName((hlim::Clock *) clock) << std::endl;

    if (risingEdge)
        m_testbenchFile << '1' << std::endl;
    else
        m_testbenchFile << '0' << std::endl;
}

void FileBasedTestbenchRecorder::onSimProcOutputOverridden(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT(name_it != m_outputToIoPinName.end());

    m_testbenchFile << "SET" << std::endl << name_it->second << std::endl;
    m_testbenchFile << state << std::endl;
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
            if (allDefined) {
                m_assertStatements << "CHECK" << std::endl << name_it->second << std::endl << state << std::endl;
            } else {
                //HCL_ASSERT_HINT(false, "Can't yet export testvector checks with partially defined vectors!");
                m_assertStatements << "CHECK" << std::endl << name_it->second << std::endl << state << std::endl;
            }
        }
    }
}


}
