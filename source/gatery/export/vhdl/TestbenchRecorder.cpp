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

#include "../../frontend/SimSigHandle.h" // For Seconds

#include "../../simulation/simProc/WaitClock.h"
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
	flush(m_simulator.getCurrentSimulationTime());// + Seconds{1,1'000'000'000'000ull});
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

	CodeFormatting &cf = m_ast->getCodeFormatting();

	declareSignals(m_testbenchFile);

	for (auto ioPin : m_allIOPins) {
		const std::string &name = rootEntity->getNamespaceScope().get(ioPin).name;

		if (ioPin->isOutputPin())
			m_outputToIoPinName[ioPin->getDriver(0)] = name;

		if (ioPin->isInputPin()) {
			hlim::NodePort pinOutput(const_cast<hlim::Node_Pin*>(ioPin), 0);
			m_outputToIoPinName[pinOutput] = name;
		}
	}

	m_testbenchFile << "BEGIN" << std::endl;

	cf.indent(m_testbenchFile, 1);
	m_testbenchFile << "inst_root : entity work." << rootEntity->getName() << "(impl) port map (" << std::endl;

	writePortmap(m_testbenchFile);
   
	cf.indent(m_testbenchFile, 1);
	m_testbenchFile << ");" << std::endl;

	for (auto &clock : m_clocksOfInterest)
		buildClockProcess(m_testbenchFile, clock);

	cf.indent(m_testbenchFile, 1);
	m_testbenchFile << "sim_process : PROCESS" << std::endl;
	cf.indent(m_testbenchFile, 1);
	m_testbenchFile << "BEGIN" << std::endl;
}

void TestbenchRecorder::writeFooter()
{
//	CodeFormatting &cf = m_ast->getCodeFormatting();
//	cf.indent(m_testbenchFile, 1);
	m_testbenchFile << "TB_testbench_is_done <= '1';" << std::endl;
	m_testbenchFile << "WAIT;" << std::endl;
	m_testbenchFile << "END PROCESS;" << std::endl;
	m_testbenchFile << "END;" << std::endl;
}



void TestbenchRecorder::onPowerOn()
{
	findClocksAndPorts();

	m_writtenSimulationTime = 0;
	m_flushIntervalStart = 0;
	m_phases.push_back({});
}

void TestbenchRecorder::onAfterPowerOn()
{
	writeHeader();
}

void TestbenchRecorder::advanceTimeTo(const hlim::ClockRational &simulationTime)
{
	auto deltaT = simulationTime - m_writtenSimulationTime;

	if (simulationTime < m_writtenSimulationTime || deltaT < Seconds{1,1'000'000'000'000ul})
		deltaT = Seconds{1,1'000'000'000'000ul}; // 1ps minimum

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
		m_writtenSimulationTime += hlim::ClockRational(t / 1'000'000, unit2denom[unit-2]);

		t = t % 1'000'000;
	}

	cf.indent(m_testbenchFile, 2);
	m_testbenchFile << "WAIT FOR " << t << ' ' << unit2str[unit] << ";\n";
	m_writtenSimulationTime += hlim::ClockRational(t, unit2denom[unit]);
}

void TestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{

}

void TestbenchRecorder::onNewPhase(size_t phase)
{
	if (phase == sim::WaitClock::AFTER) {
		flush(m_simulator.getCurrentSimulationTime());
		m_phases.back() = std::move(m_postDuringPhase); // Have all the assignments from the previous DURING phase be the first thing in the next interval
		m_phases.push_back({});
	}
}

void TestbenchRecorder::onAfterMicroTick(size_t microTick)
{
	m_phases.push_back({});
}

void TestbenchRecorder::onCommitState()
{
	// queue check for assert nodes
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

	if (m_simulator.getCurrentPhase() == sim::WaitClock::DURING)
		m_postDuringPhase.resetOverrides[rst] = assignment.str();
	else
		m_phases.back().resetOverrides[rst] = assignment.str();
}

void TestbenchRecorder::onSimProcOutputOverridden(const hlim::NodePort &output, const sim::DefaultBitVectorState &state)
{
	auto name_it = m_outputToIoPinName.find(output);
	HCL_ASSERT(name_it != m_outputToIoPinName.end());

	CodeFormatting &cf = m_ast->getCodeFormatting();
	//auto *rootEntity = m_ast->getRootEntity();

	std::stringstream str_state;

	cf.indent(str_state, 2);
	str_state << name_it->second << " <= ";

	const auto& conType = hlim::getOutputConnectionType(output);

	char sep = '"';
	if (conType.isBool())
		sep = '\'';

	str_state << sep;
	str_state << state;
	str_state << sep << ';' << std::endl;


	if (m_simulator.getCurrentPhase() == sim::WaitClock::DURING)
		m_postDuringPhase.signalOverrides[name_it->second] = str_state.str();
	else
		m_phases.back().signalOverrides[name_it->second] = str_state.str();
}

void TestbenchRecorder::onSimProcOutputRead(const hlim::NodePort &output, const sim::DefaultBitVectorState &state)
{
	hlim::NodePort drivingOutput = output;
	// find output driving output pin
	for (auto nh : output.node->exploreOutput(output.port)) {
		if (dynamic_cast<hlim::Node_Pin*>(nh.node())) {
			drivingOutput = nh.node()->getDriver(0);
			break;
		} else
			if (!nh.isSignal())
				nh.backtrack();
	}	

	auto name_it = m_outputToIoPinName.find(drivingOutput);
	HCL_ASSERT_HINT(name_it != m_outputToIoPinName.end(), "Can only record asserts for signals that are output pins!");

	CodeFormatting &cf = m_ast->getCodeFormatting();

	auto &assertStatements = m_phases.back().assertStatements;
	const auto& conType = hlim::getOutputConnectionType(drivingOutput);
	if (conType.isBool()) {
		if (state.get(sim::DefaultConfig::DEFINED, 0)) {
			cf.indent(assertStatements, 2);
			assertStatements << "ASSERT " << name_it->second << " = '" << state << "';" << std::endl;
		}
	} else {
		// If all bits are defined, assert on the entire vector, otherwise build individual asserts for each valid bit.
		bool allDefined = true;
		for (auto i : utils::Range(conType.width))
			allDefined &= state.get(sim::DefaultConfig::DEFINED, i);

		if (allDefined) {
			cf.indent(assertStatements, 2);
			assertStatements << "ASSERT " << name_it->second << " = \"" << state << "\";" << std::endl;
		} else {
			for (auto i : utils::Range(conType.width))
				if (state.get(sim::DefaultConfig::DEFINED, i)) {
					char v = state.get(sim::DefaultConfig::VALUE, i)?'1':'0';
					assertStatements << "ASSERT " << name_it->second << '('<<i<<") = '" << v << "';" << std::endl;
				}
		}
	}
}

void TestbenchRecorder::onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc)
{
	CodeFormatting &cf = m_ast->getCodeFormatting();

	flush(simulationTime);

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

	flush(simulationTime);

	cf.indent(m_testbenchFile, 2);
	m_testbenchFile << "-- End: " << id << std::endl;
	m_testbenchFile << std::endl;
}

void TestbenchRecorder::flush(const hlim::ClockRational &flushIntervalEnd)
{
	auto interval = (flushIntervalEnd - m_flushIntervalStart) / (2 + m_phases.size());

	for (auto phaseIdx : utils::Range(m_phases.size())) {
		const auto &phase = m_phases[phaseIdx];

		if (phase.assertStatements.str().empty() && phase.signalOverrides.empty() && phase.resetOverrides.empty())
			continue;

		advanceTimeTo(m_flushIntervalStart + interval * (1 + phaseIdx));

		m_testbenchFile << phase.assertStatements.str();

		for (const auto &p : phase.signalOverrides)
			m_testbenchFile << p.second;

		for (const auto &p : phase.resetOverrides)
			m_testbenchFile << p.second;
	}

	m_phases.clear();
	m_phases.push_back({});

	m_flushIntervalStart = flushIntervalEnd;
}


}
