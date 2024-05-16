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
#include "gatery/scl_pch.h"

#include "IntelQuartusTestFixture.h"

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>
#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/frontend/SynthesisTool.h>
#include <gatery/hlim/Circuit.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#pragma warning(push, 0)
#pragma warning(disable : 4018) // boost process environment "'<': signed/unsigned mismatch"
#endif
#include <boost/process.hpp>
#ifdef _WIN32
#pragma warning(pop)
#endif


#include <boost/asio.hpp>
#include <boost/format.hpp>

namespace bp = boost::process;

namespace gtry {

std::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinPath;
std::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinSynthesizer;
std::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinFitter;
std::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinAssembler;
std::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinTimingAnalyzer;


IntelQuartusGlobalFixture::IntelQuartusGlobalFixture()
{
	auto& testSuite = boost::unit_test::framework::master_test_suite();
	for (int i = 1; i+1 < testSuite.argc; i++) {
		std::string_view arg(testSuite.argv[i]);

		if (arg == "--intelQuartus")
			m_intelQuartusBinPath = testSuite.argv[i+1];
	}

	auto&& libEnv = boost::this_process::wenvironment()[L"IntelQuartus_BIN_PATH"];
	if (!libEnv.empty())
		m_intelQuartusBinPath = libEnv.to_string();



	m_intelQuartusBinSynthesizer = m_intelQuartusBinPath / "quartus_syn.exe";
	if (!std::filesystem::exists(m_intelQuartusBinSynthesizer))
		m_intelQuartusBinSynthesizer = m_intelQuartusBinPath / "quartus_syn";
	m_intelQuartusBinFitter = m_intelQuartusBinPath / "quartus_fit.exe";
	if (!std::filesystem::exists(m_intelQuartusBinFitter))
		m_intelQuartusBinFitter = m_intelQuartusBinPath / "quartus_fit";
	m_intelQuartusBinAssembler = m_intelQuartusBinPath / "quartus_asm.exe";
	if (!std::filesystem::exists(m_intelQuartusBinAssembler))
		m_intelQuartusBinAssembler = m_intelQuartusBinPath / "quartus_asm";
	m_intelQuartusBinTimingAnalyzer = m_intelQuartusBinPath / "quartus_sta.exe";
	if (!std::filesystem::exists(m_intelQuartusBinTimingAnalyzer))
		m_intelQuartusBinTimingAnalyzer = m_intelQuartusBinPath / "quartus_sta";
}

IntelQuartusGlobalFixture::~IntelQuartusGlobalFixture()
{
}

IntelQuartusTestFixture::IntelQuartusTestFixture()
{
	const auto& testCase = boost::unit_test::framework::current_test_case();
	std::filesystem::path testCaseFile{ std::string{ testCase.p_file_name.begin(), testCase.p_file_name.end() } };
	m_cwd = std::filesystem::path{ "tmp" } / testCaseFile.stem() / testCase.p_name.get();

	if (!std::filesystem::create_directories(m_cwd))
		for (auto&& f : std::filesystem::directory_iterator{ m_cwd })
			if (f.is_regular_file())
				std::filesystem::remove(f);
			else if (f.is_directory()) {
				auto stem = f.path().stem();
				if (stem != "." && stem != "..")
					std::filesystem::remove_all(f.path());
			}

	// Default to cyclone 10 as that is the default device in quartus pro
	auto device = std::make_unique<gtry::scl::IntelDevice>();
	device->setupCyclone10();
	design.setTargetTechnology(std::move(device));
}

IntelQuartusTestFixture::~IntelQuartusTestFixture()
{
}

void IntelQuartusTestFixture::addCustomVHDL(std::string name, std::string content)
{
	m_customVhdlFiles[std::move(name)] = std::move(content);
}

void IntelQuartusTestFixture::testCompilation()
{
	design.postprocess();

	m_vhdlExport.emplace(m_cwd / "design.vhd");
	for (const auto &file_content : m_customVhdlFiles)
		m_vhdlExport->addCustomVhdlFile(file_content.first, file_content.second);

	m_vhdlExport->outputMode(vhdlOutputMode);

	m_vhdlExport->targetSynthesisTool(new IntelQuartus());
	m_vhdlExport->writeStandAloneProjectFile("project.qsf");
	m_vhdlExport->writeConstraintsFile("constraints.sdc");
	m_vhdlExport->writeClocksFile("clocks.sdc");
	(*m_vhdlExport)(design.getCircuit());

	m_generatedSourceFiles = SynthesisTool::sourceFiles(*m_vhdlExport, true, false);
	
	BOOST_CHECK(bp::system(IntelQuartusGlobalFixture::getIntelQuartusBinSynthesizer().string(), bp::start_dir(m_cwd.string()), "project", bp::std_out.null()) == 0);
	BOOST_CHECK(bp::system(IntelQuartusGlobalFixture::getIntelQuartusBinFitter().string(), bp::start_dir(m_cwd.string()), "project", bp::std_out.null()) == 0);
	BOOST_CHECK(bp::system(IntelQuartusGlobalFixture::getIntelQuartusBinAssembler().string(), bp::start_dir(m_cwd.string()), "project", bp::std_out.null()) == 0);
	BOOST_CHECK(bp::system(IntelQuartusGlobalFixture::getIntelQuartusBinTimingAnalyzer().string(), bp::start_dir(m_cwd.string()), "project", bp::std_out.null()) == 0);

	writeTimingToCsvTclScript();
	BOOST_CHECK(bp::system(IntelQuartusGlobalFixture::getIntelQuartusBinTimingAnalyzer().string(), bp::start_dir(m_cwd.string()), "-t", "dumpTiming.tcl", bp::std_out.null()) == 0);
	parseTimingCsv();
}

bool IntelQuartusTestFixture::exportContains(const std::regex &regex)
{
	for (std::filesystem::path& vhdlFile : m_generatedSourceFiles) {
		std::ifstream file((m_cwd / vhdlFile).string().c_str(), std::fstream::binary);
		BOOST_TEST((bool) file);
		std::stringstream buffer;
		buffer << file.rdbuf();

		if (std::regex_search(buffer.str(), regex))
			return true;
	}
	return false;
}


const std::map<std::string, IntelQuartusTestFixture::FitterResourceUtilization, std::less<>> &IntelQuartusTestFixture::getFitterResourceUtilization()
{
	if (m_resourceUtilization)
		return *m_resourceUtilization;

	std::ifstream file((m_cwd / "output_files" / "project.fit.place.rpt").string().c_str(), std::fstream::binary);
	BOOST_TEST((bool) file);
	std::stringstream buffer;
	buffer << file.rdbuf();


	std::regex extractSummaryLines{"Fitter Resource Utilization by Entity\\s*;[\r\n]*[-+]*[\r\n]*;[^\r\n]*[\r\n]*[+-]*[\r\n]*([^+-]*)"};

	std::smatch fullMatch;
	std::string bufferStr = buffer.str();
	bool matchSuccessfull = std::regex_search(bufferStr, fullMatch, extractSummaryLines);
	HCL_ASSERT_HINT(matchSuccessfull, "Could not find Fitter Resource Utilization in report, potentially the reporting format changed!");
	
	std::string summaryLines = fullMatch[1];

	std::regex extractFields{";([^;]*)" // ; Compilation Hierarchy Node
							 ";\\s*(\\d*.\\d*)\\s+\\((\\d*.\\d*)\\)\\s*" // ; ALMs needed [=A-B+C]
							 ";\\s*(\\d*.\\d*)\\s+\\((\\d*.\\d*)\\)\\s*" // ; [A] ALMs used in final placement
							 ";\\s*(\\d*.\\d*)\\s+\\((\\d*.\\d*)\\)\\s*" // ; [B] Estimate of ALMs recoverable by dense packing
							 ";\\s*(\\d*.\\d*)\\s+\\((\\d*.\\d*)\\)\\s*" // ; [C] Estimate of ALMs unavailable
							 ";\\s*(\\d*.\\d*)\\s+\\((\\d*.\\d*)\\)\\s*" // ; ALMs used for memory
							 ";\\s*(\\d*)\\s+\\((\\d+)\\)\\s*" // ; Combinational ALUTs
							 ";\\s*(\\d*)\\s+\\((\\d+)\\)\\s*" // ; Dedicated Logic Registers
							 ";\\s*(\\d*)\\s+\\((\\d+)\\)\\s*" // ; I/O Registers	
							 ";\\s*(\\d*)\\s*" // ; Block Memory bits
							 ";\\s*(\\d*)\\s*" // ; M20Ks	
							 ";\\s*(\\d*)\\s*" // ; DSPs needed	
							 ";\\s*(\\d*)\\s*" // ; DSPs used	
							 ";\\s*(\\d*)\\s*" // ; DSPs recoverable	
							 ";\\s*(\\d*)\\s*" // ; Pins
							 ";\\s*(\\d*)\\s*" // ; Virtual Pins
							 ";\\s*(\\d*)\\s+\\((\\d+)\\)\\s*" // ; IO PLLs
							 ";\\s*([^\\s]*)\\s*" // ; Full name
							 ";\\s*([^\\s]*)\\s*" // ; Entity Name 
							 ";\\s*([^\\s]*)\\s*" // ; Library Name
							 };

	m_resourceUtilization.emplace();
    for (std::smatch m; std::regex_search(summaryLines, m, extractFields); summaryLines = m.suffix()) {
		FitterResourceUtilization res;

		res.ALMsNeeded = 		  		{ std::stod(m[2]), std::stod(m[3]) };
		res.ALMsInFinalPlacement = 		{ std::stod(m[4]), std::stod(m[5]) };
		res.ALMsRecoverable = 	  		{ std::stod(m[6]), std::stod(m[7]) };
		res.ALMsUnavailable = 	  		{ std::stod(m[8]), std::stod(m[9]) };
		res.ALMsForMemory = 	  		{ std::stod(m[10]), std::stod(m[11]) };
		res.combinationalALUTs =   		{ (size_t) std::stoi(m[12]), (size_t) std::stoi(m[13]) };
		res.dedicatedLogicRegisters = 	{ (size_t) std::stoi(m[14]), (size_t) std::stoi(m[15]) };
		res.ioRegisters = 				{ (size_t) std::stoi(m[16]), (size_t) std::stoi(m[17]) };

		res.blockMemoryBits = 			(size_t) std::stoi(m[18]);
		res.M20Ks = 					(size_t) std::stoi(m[19]);
		res.DSPsNeeded = 				(size_t) std::stoi(m[20]);
		res.DSPsInFinalPlacement = 		(size_t) std::stoi(m[21]);
		res.DSPsRecoverable = 			(size_t) std::stoi(m[22]);
		res.fullHierarchyName = 		m[27];

		(*m_resourceUtilization)[res.fullHierarchyName] = res;
    }

	return *m_resourceUtilization;
}


IntelQuartusTestFixture::FitterResourceUtilization IntelQuartusTestFixture::getFitterResourceUtilization(std::string_view instancePath)
{
	const auto &utilization = getFitterResourceUtilization();
	auto it = utilization.find(instancePath);
	HCL_DESIGNCHECK_HINT(it != utilization.end(), "Could not find specified instance within fitter resoure utilization report!");
	return it->second;
}


/*
std::vector<IntelQuartusTestFixture::FitterRAMSummary> IntelQuartusTestFixture::getFitterRAMSummary()
{
}
*/


void IntelQuartusTestFixture::writeTimingToCsvTclScript()
{
	std::ofstream file((m_cwd / "dumpTiming.tcl").string().c_str(), std::fstream::binary);
	file << R"Delim(
project_open project

create_timing_netlist
read_sdc clocks.sdc
read_sdc constraints.sdc
update_timing_netlist

set domain_list [get_clock_fmax_info]

set csvFile [open "timing.csv" w]

foreach domain $domain_list {
    set name [lindex $domain 0]
    set fmax [lindex $domain 1]
    set restricted_fmax [lindex $domain 2]

    puts $csvFile "$name;$fmax;$restricted_fmax"
}

close $csvFile

project_close
)Delim";
}

void IntelQuartusTestFixture::parseTimingCsv()
{
	m_timingAnalysisFmax.clear();

	std::ifstream file((m_cwd / "timing.csv").string().c_str(), std::fstream::binary);
	std::string line;
	while (std::getline(file, line)) {
		if (line.empty()) continue;

		std::vector<std::string> elems;
		boost::split(elems, line, boost::is_any_of(";\r\n"));

		m_timingAnalysisFmax[elems[0]] = TimingAnalysisFMax{
			.fmax = std::stod(elems[1]),
			.fmaxRestricted = std::stod(elems[2]),
		};
	}
}

IntelQuartusTestFixture::TimingAnalysisFMax IntelQuartusTestFixture::getTimingAnalysisFmax(hlim::Clock *clock) const
{
	const std::string &vhdlName = m_vhdlExport->getAST()->getNamespaceScope().getClock(clock->getClockPinSource()).name;
	auto it = m_timingAnalysisFmax.find(vhdlName);
	HCL_DESIGNCHECK_HINT(it != m_timingAnalysisFmax.end(), "Quartus did not report on the timing of the specified clock!");
	return it->second;
}

bool IntelQuartusTestFixture::timingMet(hlim::Clock *clock) const
{
	auto fmax = getTimingAnalysisFmax(clock);
	return fmax.fmaxRestricted >= gtry::hlim::toDouble(clock->absoluteFrequency()) * 1e-6;
}


}
