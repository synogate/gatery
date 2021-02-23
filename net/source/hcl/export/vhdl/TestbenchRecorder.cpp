#include "TestbenchRecorder.h"

#include "AST.h"
#include "Entity.h"

#include "../../simulation/Simulator.h"

#include "../../hlim/coreNodes/Node_Pin.h"



#include <sstream>
#include <iostream>

namespace hcl::core::vhdl {


TestbenchRecorder::TestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, const std::string &filename) : m_exporter(exporter), m_ast(ast), m_simulator(simulator)
{
    m_testbenchFile.open(filename.c_str(), std::fstream::out);
    writeHeader();
}

TestbenchRecorder::~TestbenchRecorder()
{
    writeFooter();
}

void TestbenchRecorder::writeHeader()
{
    m_testbenchFile << R"Delim(
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;

ENTITY testbench IS
END testbench;

ARCHITECTURE tb OF testbench IS

)Delim";

    auto *rootEntity = m_ast->getRootEntity();

    const auto &allClocks = rootEntity->getClocks();
    const auto &allIOPins = rootEntity->getIoPins();

    CodeFormatting &cf = m_ast->getCodeFormatting();

    for (auto clock : allClocks) {
        m_testbenchFile << "    SIGNAL " << rootEntity->getNamespaceScope().getName(clock) << " : STD_LOGIC;" << std::endl;
        if (clock->getResetType() != hlim::Clock::ResetType::NONE)
            m_testbenchFile << "    SIGNAL " << rootEntity->getNamespaceScope().getName(clock)<<clock->getResetName() << " : STD_LOGIC;" << std::endl;
    }


    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);
        bool isInput = !ioPin->getDirectlyDriven(0).empty();
        bool isOutput = ioPin->getNonSignalDriver(0).node != nullptr;

        hlim::ConnectionType conType;
        if (isOutput) {
            auto driver = ioPin->getNonSignalDriver(0);
            conType = driver.node->getOutputConnectionType(driver.port);
        } else
            conType = ioPin->getOutputConnectionType(0);

        m_testbenchFile << "    SIGNAL " << name << " : ";
        cf.formatConnectionType(m_testbenchFile, conType);
        m_testbenchFile << ';' << std::endl;

        if (isOutput)
            m_outputToIoPinName[ioPin->getDriver(0)] = name;

        if (isInput)
            m_outputToIoPinName[{.node=ioPin, .port=0}] = name;

    }
    m_testbenchFile << "BEGIN" << std::endl;

    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "inst_root : entity work." << rootEntity->getName() << "(impl) port map (" << std::endl;

    std::vector<std::string> portmapList;

    for (auto &s : allClocks) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getName(s) << " => ";
        line << rootEntity->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
        if (s->getResetType() != hlim::Clock::ResetType::NONE) {
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
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << portmapList[i];
        if (i+1 < portmapList.size())
            m_testbenchFile << ",";
        m_testbenchFile << std::endl;
    }


    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << ");" << std::endl;

    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "sim_process : PROCESS" << std::endl;
    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "BEGIN" << std::endl;


    for (auto &s : allClocks) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << rootEntity->getNamespaceScope().getName(s) << " <= '0';" << std::endl;
        if (s->getResetType() != hlim::Clock::ResetType::NONE) {
            cf.indent(m_testbenchFile, 2);
            m_testbenchFile << rootEntity->getNamespaceScope().getName(s)<<s->getResetName() << " <= '1';" << std::endl;
        }
    }

    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "WAIT FOR 1 us;" << std::endl;
    for (auto &s : allClocks) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << rootEntity->getNamespaceScope().getName(s) << " <= '1';" << std::endl;
    }
    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "WAIT FOR 1 us;" << std::endl;

    for (auto &s : allClocks) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << rootEntity->getNamespaceScope().getName(s) << " <= '0';" << std::endl;
        if (s->getResetType() != hlim::Clock::ResetType::NONE) {
            cf.indent(m_testbenchFile, 2);
            m_testbenchFile << rootEntity->getNamespaceScope().getName(s)<<s->getResetName() << " <= '0';" << std::endl;
        }
    }
    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << "WAIT FOR 1 us;" << std::endl;

    m_lastSimulationTime = 0;
}

void TestbenchRecorder::writeFooter()
{
//    CodeFormatting &cf = m_ast->getCodeFormatting();
//    cf.indent(m_testbenchFile, 1);
    m_testbenchFile << "END PROCESS;" << std::endl;
    m_testbenchFile << "END;" << std::endl;
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

void TestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();

    auto timeDiff = simulationTime - m_lastSimulationTime;
    m_lastSimulationTime = simulationTime;

    // All asserts are collected to be triggered halfway between the last tick (when signals were set) and the next tick (when new stuff happens).
    if (m_assertStatements.str().empty()) {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << "WAIT FOR ";
        formatTime(m_testbenchFile, timeDiff);
        m_testbenchFile << ';' << std::endl;
    } else {
        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << "WAIT FOR ";
        formatTime(m_testbenchFile, timeDiff * hlim::ClockRational(1,2));
        m_testbenchFile << ';' << std::endl;

        m_testbenchFile << m_assertStatements.str();
        m_assertStatements.str(std::string());

        cf.indent(m_testbenchFile, 2);
        m_testbenchFile << "WAIT FOR ";
        formatTime(m_testbenchFile, timeDiff * hlim::ClockRational(1,2));
        m_testbenchFile << ';' << std::endl;
    }
}

void TestbenchRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
    CodeFormatting &cf = m_ast->getCodeFormatting();
    auto *rootEntity = m_ast->getRootEntity();

    cf.indent(m_testbenchFile, 2);
    if (risingEdge)
        m_testbenchFile << rootEntity->getNamespaceScope().getName((hlim::Clock *) clock) << " <= '1';" << std::endl;
    else
        m_testbenchFile << rootEntity->getNamespaceScope().getName((hlim::Clock *) clock) << " <= '0';" << std::endl;
}

void TestbenchRecorder::onSimProcOutputOverridden(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT(name_it != m_outputToIoPinName.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();
    auto *rootEntity = m_ast->getRootEntity();

    cf.indent(m_testbenchFile, 2);
    m_testbenchFile << name_it->second << " <= ";

    const auto& conType = output.node->getOutputConnectionType(output.port);

    char sep = '"';
    if (conType.interpretation == hlim::ConnectionType::BOOL)
        sep = '\'';

    m_testbenchFile << sep;
    m_testbenchFile << state;
    m_testbenchFile << sep << ';' << std::endl;
}

void TestbenchRecorder::onSimProcOutputRead(hlim::NodePort output, const sim::DefaultBitVectorState &state)
{
    auto name_it = m_outputToIoPinName.find(output);
    HCL_ASSERT(name_it != m_outputToIoPinName.end());

    CodeFormatting &cf = m_ast->getCodeFormatting();

    const auto& conType = output.node->getOutputConnectionType(output.port);
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


}