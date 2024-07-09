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

#include "../../frontend/SimSigHandle.h" // For Seconds
#include "../../simulation/Simulator.h"

#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_Signal.h"

#include "../../utils/FileSystem.h"

#include <sstream>
#include <iostream>

namespace gtry::vhdl {


FileBasedTestbenchRecorder::FileBasedTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, utils::FileSystem &fileSystem, std::string name) : BaseTestbenchRecorder(ast, simulator, std::move(name)), m_exporter(exporter)
{
	m_dependencySortedEntities.push_back(m_entityName);
	m_testVectorFilename = m_name + ".testvectors";
	m_auxiliaryDataFiles.push_back(m_testVectorFilename);

	m_testvectorFile = fileSystem.writeFile(m_testVectorFilename);
	m_testbenchFile = fileSystem.writeFile(m_ast->getFilename(m_name));

	auto verilogTBFilename = m_ast->getFilename(m_name);
	verilogTBFilename.replace_extension("v");
	m_verilogTestbenchFile = fileSystem.writeFile(verilogTBFilename);
}

FileBasedTestbenchRecorder::~FileBasedTestbenchRecorder()
{
	flush(m_simulator.getCurrentSimulationTime());// + Seconds{1,1'000'000'000'000ull});
}

void FileBasedTestbenchRecorder::writeVHDL()
{
	auto &vhdlFile = m_testbenchFile->stream();

	vhdlFile << R"(
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.all;
USE std.textio.all;

ENTITY )" << m_entityName << R"( IS
END )" <<m_entityName << R"(;

ARCHITECTURE tb OF )" << m_entityName << R"( IS

)";

	auto *rootEntity = m_ast->getRootEntity();

	CodeFormatting &cf = m_ast->getCodeFormatting();

	declareSignals(vhdlFile);

	utils::StableMap<hlim::NodePort, bool> outputIsBool;
	utils::StableSet<hlim::NodePort> outputIsDrivenByNetwork;

	for (auto ioPin : m_allIOPins) {
		const std::string &name = rootEntity->getNamespaceScope().get(ioPin).name;
		auto conType = ioPin->getConnectionType();

		if (ioPin->isOutputPin()) {
			m_outputToIoPinName[ioPin->getDriver(0)] = name;
			outputIsBool[ioPin->getDriver(0)] = conType.isBool();
			outputIsDrivenByNetwork.insert(ioPin->getDriver(0));
		}

		if (ioPin->isInputPin()) {
			hlim::NodePort pinOutput{const_cast<hlim::Node_Pin*>(ioPin), 0};
			m_outputToIoPinName[pinOutput] = name;
			outputIsBool[pinOutput] = conType.isBool();
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
	

	//const std::string assertionSeverity = "warning";
	const std::string assertionSeverity = "error";

	vhdlFile << "BEGIN" << std::endl;

	cf.indent(vhdlFile, 1);
	vhdlFile << "inst_root : entity work." << rootEntity->getName() << "(impl) port map (" << std::endl;

	writePortmap(vhdlFile);

	cf.indent(vhdlFile, 1);
	vhdlFile << ");" << std::endl;

	for (auto &clock : m_clocksOfInterest)
		buildClockProcess(vhdlFile, clock);	

	cf.indent(vhdlFile, 1);
	vhdlFile << "sim_process : PROCESS" << std::endl;

	for (auto ioPin : m_allIOPins) {
		const auto &decl = rootEntity->getNamespaceScope().get(ioPin);

		vhdlFile << "	VARIABLE v_";
		cf.formatDeclaration(vhdlFile, decl);
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

	cf.indent(vhdlFile, 2);
	vhdlFile << "file_open(test_vector_file, \"" << m_testVectorFilename << "\", read_mode);" << std::endl;

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
			vhdlFile << "ASSERT std_match(" << p.second << ", v_" << p.second << ") severity " << assertionSeverity<< ";" << std::endl;
		}
		cf.indent(vhdlFile, 4);
		vhdlFile << "ELSE" << std::endl;
		cf.indent(vhdlFile, 5);
		vhdlFile << "REPORT \"An error occured while parsing the test vector file: unknown signal:\" & v_line(1 to v_line'length);" << std::endl;
		cf.indent(vhdlFile, 5);
		vhdlFile << "ASSERT FALSE severity failure;" << std::endl;

		cf.indent(vhdlFile, 4);
		vhdlFile << "END IF;" << std::endl;
#if 0
	cf.indent(vhdlFile, 3);
	vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"CLK\") THEN" << std::endl;

		cf.indent(vhdlFile, 4);
		vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

		cf.indent(vhdlFile, 4);
		vhdlFile << "IF false THEN" << std::endl;
		for (auto c : m_clocksOfInterest) {

			auto *rootEntity = m_ast->getRootEntity();
			std::string clockName =  rootEntity->getNamespaceScope().getClock((hlim::Clock *) c).name;


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
#endif
	cf.indent(vhdlFile, 3);
	vhdlFile << "ELSIF stringcompare(v_line(1 to v_line'length), \"RST\") THEN" << std::endl;

		cf.indent(vhdlFile, 4);
		vhdlFile << "readline(test_vector_file, v_line);" << std::endl;

		cf.indent(vhdlFile, 4);
		vhdlFile << "IF false THEN" << std::endl;
		for (auto c : m_resetsOfInterest) {

			auto *rootEntity = m_ast->getRootEntity();
			std::string resetName =  rootEntity->getNamespaceScope().getReset((hlim::Clock *) c).name;


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


	vhdlFile << "TB_testbench_is_done <= '1';" << std::endl;
	vhdlFile << "WAIT;" << std::endl;
	vhdlFile << "END PROCESS;" << std::endl;
	vhdlFile << "END;" << std::endl;
}

void FileBasedTestbenchRecorder::onPowerOn()
{
	findClocksAndPorts();

	m_writtenSimulationTime = 0;
	m_flushIntervalStart = 0;
	m_phases.push_back({});
}

void FileBasedTestbenchRecorder::onAfterPowerOn()
{
	writeVHDL();
	writeVerilogTestbench();
}



void FileBasedTestbenchRecorder::onNewTick(const hlim::ClockRational &simulationTime)
{
}

void FileBasedTestbenchRecorder::onNewPhase(size_t phase)
{
	if (phase == sim::WaitClock::AFTER) {
		flush(m_simulator.getCurrentSimulationTime());
		m_phases.back() = std::move(m_postDuringPhase); // Have all the assignments from the previous DURING phase be the first thing in the next interval
		m_phases.push_back({});
	}
}

void FileBasedTestbenchRecorder::onAfterMicroTick(size_t microTick)
{
	m_phases.push_back({});
}

void FileBasedTestbenchRecorder::onCommitState()
{
	// queue check for assert nodes
}



void FileBasedTestbenchRecorder::advanceTimeTo(const hlim::ClockRational &simulationTime)
{
	auto timeDiffInPS = (simulationTime - m_writtenSimulationTime) * 1'000'000'000'000ull;
	std::uint64_t roundedTimeDiffInPS = timeDiffInPS.numerator() / timeDiffInPS.denominator();

	m_testvectorFile->stream() << "ADV\n" << roundedTimeDiffInPS << std::endl;
	m_writtenSimulationTime += Seconds{roundedTimeDiffInPS, 1'000'000'000'000ull};
}

void FileBasedTestbenchRecorder::flush(const hlim::ClockRational &flushIntervalEnd)
{
	auto interval = (flushIntervalEnd - m_flushIntervalStart) / (2 + m_phases.size());

	for (auto phaseIdx : utils::Range(m_phases.size())) {
		const auto &phase = m_phases[phaseIdx];

		if (phase.assertStatements.str().empty() && phase.signalOverrides.empty() && phase.resetOverrides.empty())
			continue;

		advanceTimeTo(m_flushIntervalStart + interval * (1 + phaseIdx));

		m_testvectorFile->stream() << phase.assertStatements.str();
		
		for (const auto &p : phase.signalOverrides) 
			m_testvectorFile->stream() << "SET\n" << p.first << '\n' << p.second << '\n';

		for (const auto &p : phase.resetOverrides) 
			m_testvectorFile->stream() << "RST\n" << p.first << '\n' << p.second << '\n';
	}

	m_phases.clear();
	m_phases.push_back({});

	m_flushIntervalStart = flushIntervalEnd;
}

void FileBasedTestbenchRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
#if 0
	if (!m_clocksOfInterest.contains(clock)) return;

	auto *rootEntity = m_ast->getRootEntity();

	m_testvectorFile->stream() << "CLK\n" << rootEntity->getNamespaceScope().getClock((hlim::Clock *) clock).name << std::endl;

	if (risingEdge)
		m_testvectorFile->stream() << "1\n";
	else
		m_testvectorFile->stream() << "0\n";
#endif
}

void FileBasedTestbenchRecorder::onReset(const hlim::Clock *clock, bool resetAsserted)
{
	if (!m_resetsOfInterest.contains(clock)) return;

	auto *rootEntity = m_ast->getRootEntity();

	const std::string rst = rootEntity->getNamespaceScope().getReset((hlim::Clock *) clock).name;

	if (m_simulator.getCurrentPhase() == sim::WaitClock::DURING)
		m_postDuringPhase.resetOverrides[rst] = resetAsserted?"1":"0";
	else
		m_phases.back().resetOverrides[rst] = resetAsserted?"1":"0";
}

void FileBasedTestbenchRecorder::onSimProcOutputOverridden(const hlim::NodePort &output, const sim::ExtendedBitVectorState &state)
{
	const auto *pin = dynamic_cast<const hlim::Node_Pin*>(output.node);
	HCL_ASSERT(pin);
	if (pin->getPinNodeParameter().simulationOnlyPin)
		return;

	auto name_it = m_outputToIoPinName.find(output);
	HCL_ASSERT(name_it != m_outputToIoPinName.end());

	std::stringstream str_state;
	str_state << state;

	if (m_simulator.getCurrentPhase() == sim::WaitClock::DURING)
		m_postDuringPhase.signalOverrides[name_it->second] = str_state.str();
	else
		m_phases.back().signalOverrides[name_it->second] = str_state.str();
}

void FileBasedTestbenchRecorder::onSimProcOutputRead(const hlim::NodePort &output, const sim::DefaultBitVectorState &state)
{
	hlim::NodePort drivingOutput = output;
	// find output driving output pin
	bool foundSimulationOnlyOutput = false;
	bool foundNonSimulationOnlyOutput = false;	
	for (auto nh : output.node->exploreOutput(output.port)) {
		if (auto *pin = dynamic_cast<hlim::Node_Pin*>(nh.node())) {
			drivingOutput = nh.node()->getDriver(0);
			if (pin->getPinNodeParameter().simulationOnlyPin)
				foundSimulationOnlyOutput = true;
			else
				foundNonSimulationOnlyOutput = true;

			break;
		} else
			if (!nh.isSignal())
				nh.backtrack();
	}	

	if (foundSimulationOnlyOutput && !foundNonSimulationOnlyOutput)
		return;

	auto name_it = m_outputToIoPinName.find(drivingOutput);
	if (name_it == m_outputToIoPinName.end()) {
		if (isDrivenByPin(drivingOutput)) {
			// It is legal to read back the value previously set to an input pin, e.g. in order to drive simulation processes machinery.
			// In this case we can skip emitting an assert since technically there is nothing to check here.
			return;
		}
		HCL_ASSERT_HINT(name_it != m_outputToIoPinName.end(), "Can only record asserts for signals that are output pins!");
	}

	const auto& conType = hlim::getOutputConnectionType(drivingOutput);
	if (conType.isBool()) {
		if (state.get(sim::DefaultConfig::DEFINED, 0)) {
			m_phases.back().assertStatements << "CHECK" << std::endl << name_it->second << std::endl << state << std::endl;
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
			m_phases.back().assertStatements << "CHECK" << std::endl << name_it->second << std::endl;
			for (int i = (int)conType.width - 1; i >= 0; i--) {
				bool d = state.get(sim::DefaultConfig::DEFINED, i);
				bool v = state.get(sim::DefaultConfig::VALUE, i);
				if (d)
					m_phases.back().assertStatements << (v?'1':'0');
				else
					m_phases.back().assertStatements << '-';
			}
			m_phases.back().assertStatements << std::endl;
		}
	}
}




void FileBasedTestbenchRecorder::writeVerilogTestbench()
{
	auto &verilogFile = m_verilogTestbenchFile->stream();

	verilogFile << R"(
`timescale 1ps/1ps

module )" << m_dependencySortedEntities.back() << "();" << std::endl;


	auto *rootEntity = m_ast->getRootEntity();

	CodeFormatting &cf = m_ast->getCodeFormatting();

	declareSignalsVerilog(verilogFile);

	utils::StableMap<hlim::NodePort, bool> outputIsBool;
	utils::StableSet<hlim::NodePort> outputIsDrivenByNetwork;
	utils::StableMap<hlim::NodePort, bool> isBidirectional;

	for (auto ioPin : m_allIOPins) {
		const std::string &name = rootEntity->getNamespaceScope().get(ioPin).name;
		auto conType = ioPin->getConnectionType();

		if (ioPin->isOutputPin()) {
			m_outputToIoPinName[ioPin->getDriver(0)] = name;
			outputIsBool[ioPin->getDriver(0)] = conType.isBool();
			outputIsDrivenByNetwork.insert(ioPin->getDriver(0));
		}

		if (ioPin->isInputPin()) {
			hlim::NodePort pinOutput{const_cast<hlim::Node_Pin*>(ioPin), 0};
			m_outputToIoPinName[pinOutput] = name;
			outputIsBool[pinOutput] = conType.isBool();
			isBidirectional[pinOutput] = ioPin->isBiDirectional();
		}

	}

	verilogFile << R"(
    function automatic integer stringcompare;
        input string v_line;
        input string str;
        integer i;
        begin
            if ($strlen(v_line) != $strlen(str)) begin
                stringcompare = 0;
                return;
            end
            for (i = 0; i < $strlen(v_line); i = i + 1) begin
                if (v_line[i] != str[i]) begin
                    stringcompare = 0;
                    return;
                end
            end
            stringcompare = 1;
        end
    endfunction
	)";
	

	const std::string assertionSeverity = "warning";


	cf.indent(verilogFile, 1);
	verilogFile << rootEntity->getName() << " inst_root (" << std::endl;

	writePortmapVerilog(verilogFile);

	cf.indent(verilogFile, 1);
	verilogFile << ");" << std::endl;

	for (auto &clock : m_clocksOfInterest)
		buildClockProcessVerilog(verilogFile, clock);	

	cf.indent(verilogFile, 1);
	verilogFile << "initial begin" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "reg [4095:0] line;" << std::endl;
	cf.indent(verilogFile, 2);
	verilogFile << "integer time_in_ps;" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "integer test_vector_file;" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "reg v_clk;" << std::endl;

	for (auto ioPin : m_allIOPins) {
		const auto &decl = rootEntity->getNamespaceScope().get(ioPin);

		cf.indent(verilogFile, 2);
		verilogFile << "reg ";
		formatDeclarationVerilog(verilogFile, decl);
		verilogFile << "_TB_helper;" << std::endl;
	}


	cf.indent(verilogFile, 2);
	verilogFile << "test_vector_file = $fopen(\"" << m_testVectorFilename << "\", \"r\");" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "if (test_vector_file == 0) begin" << std::endl;
	cf.indent(verilogFile, 3);
	verilogFile << "$display(\"The test vector file could not be opened!\");" << std::endl;
	cf.indent(verilogFile, 3);
	verilogFile << "$stop;" << std::endl;
	cf.indent(verilogFile, 2);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "while (!$feof(test_vector_file)) begin" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "$fgets(line, test_vector_file);" << std::endl;
	
	cf.indent(verilogFile, 3);
	verilogFile << "if (stringcompare(line, \"SET\")) begin" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "$fgets(line, test_vector_file);" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "if (0) ;" << std::endl;
		for (auto &p : m_outputToIoPinName) {
			if (outputIsDrivenByNetwork.contains(p.first)) continue;

			cf.indent(verilogFile, 4);
			verilogFile << "else if (stringcompare(line, \"" << p.second << "\")) begin" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$fgets(line, test_vector_file);" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$sscanf(line, \"%b\", " << p.second << "_TB_helper);" << std::endl;

			cf.indent(verilogFile, 5);

#if 0
			if (isBidirectional[p.first])
				verilogFile << p.second << "_TB_driver <= " << p.second << "_TB_helper;" << std::endl;
			else 
#endif
				verilogFile << p.second << " <= " << p.second << "_TB_helper;" << std::endl;

			cf.indent(verilogFile, 4);
			verilogFile << "end" << std::endl;
		}
		cf.indent(verilogFile, 4);
		verilogFile << "else begin" << std::endl;
		cf.indent(verilogFile, 5);
		verilogFile << "$display(\"An error occured while parsing the test vector file: unknown signal: %s\", line);" << std::endl;
		cf.indent(verilogFile, 5);
		verilogFile << "$finish;" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "else if (stringcompare(line, \"CHECK\")) begin" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "$fgets(line, test_vector_file);" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "if (0) ;" << std::endl;
		for (auto &p : m_outputToIoPinName) {

			cf.indent(verilogFile, 4);
			verilogFile << "else if (stringcompare(line, \"" << p.second << "\")) begin" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$fgets(line, test_vector_file);" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$sscanf(line, \"%b\", " << p.second << "_TB_helper);" << std::endl;
			cf.indent(verilogFile, 5);
			verilogFile << "if (" << p.second << " !== " << p.second << "_TB_helper) $fatal(1, \"Check failed on " << p.second << "\");" << std::endl;

			cf.indent(verilogFile, 4);
			verilogFile << "end" << std::endl;
		}
		cf.indent(verilogFile, 4);
		verilogFile << "else begin" << std::endl;
		cf.indent(verilogFile, 5);
		verilogFile << "$display(\"An error occured while parsing the test vector file: unknown signal: %s\", line);" << std::endl;
		cf.indent(verilogFile, 5);
		verilogFile << "$finish;" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "else if (stringcompare(line, \"RST\")) begin" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "$fgets(line, test_vector_file);" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "if (0) ;" << std::endl;
		for (auto c : m_resetsOfInterest) {

			auto *rootEntity = m_ast->getRootEntity();
			std::string resetName =  rootEntity->getNamespaceScope().getReset((hlim::Clock *) c).name;


			cf.indent(verilogFile, 4);
			verilogFile << "else if (stringcompare(line, \"" << resetName << "\")) begin" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$fgets(line, test_vector_file);" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << "$sscanf(line, \"%b\", v_clk);" << std::endl;

			cf.indent(verilogFile, 5);
			verilogFile << resetName << " <= v_clk;" << std::endl;

			cf.indent(verilogFile, 4);
			verilogFile << "end" << std::endl;
		}
		cf.indent(verilogFile, 4);
		verilogFile << "else begin" << std::endl;

		cf.indent(verilogFile, 5);
		verilogFile << "$display(\"An error occured while parsing the test vector file: unknown clock: %s\", line);" << std::endl;
		cf.indent(verilogFile, 5);
		verilogFile << "$finish;" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "else if (stringcompare(line, \"ADV\")) begin" << std::endl;

		cf.indent(verilogFile, 4);
		verilogFile << "$fgets(line, test_vector_file);" << std::endl;
		cf.indent(verilogFile, 4);
		verilogFile << "$sscanf(line, \"%d\", time_in_ps);" << std::endl;
		cf.indent(verilogFile, 4);
		verilogFile << "#time_in_ps;" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "else begin" << std::endl;


	cf.indent(verilogFile, 4);
	verilogFile << "$display(\"An error occured while parsing the test vector file: Can't parse line: %s\", line);" << std::endl;
	cf.indent(verilogFile, 4);
	verilogFile << "$finish;" << std::endl;

	cf.indent(verilogFile, 3);
	verilogFile << "end" << std::endl;

	cf.indent(verilogFile, 2);
	verilogFile << "end" << std::endl;


	verilogFile << "TB_testbench_is_done <= 1;" << std::endl;
	verilogFile << "$stop;" << std::endl;
	verilogFile << "end" << std::endl;
	verilogFile << "endmodule" << std::endl;
}


}
