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

#include "IntelQuartusTestFixture.h"

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>
#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/frontend/SynthesisTool.h>
#include <gatery/hlim/Circuit.h>
#include <boost/test/unit_test.hpp>

#include <boost/test/unit_test.hpp>

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

namespace gtry::scl {

boost::filesystem::path IntelQuartusGlobalFixture::m_intelQuartusBinPath;


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
			if(f.is_regular_file())
				std::filesystem::remove(f);

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
	m_vhdlExport.reset();
/*
	boost::filesystem::path ghdlExecutable = bp::search_path("ghdl");

	for (std::filesystem::path& vhdlFile : m_generatedSourceFiles)
		BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, vhdlFile.string()) == 0);

//	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, "design.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-e", "--ieee=synopsys", m_ghdlArgs, "top") == 0);
*/
}

void IntelQuartusTestFixture::prepRun()
{
	design.postprocess();
	BoostUnitTestSimulationFixture::prepRun();

	m_vhdlExport.emplace(m_cwd / "design.vhd");
	m_vhdlExport->outputMode(vhdlOutputMode);
	m_vhdlExport->addTestbenchRecorder(getSimulator(), "testbench", true);
	m_vhdlExport->targetSynthesisTool(new IntelQuartus());
	m_vhdlExport->writeStandAloneProjectFile("project.qsf");
	m_vhdlExport->writeConstraintsFile("constraints.sdc");
	m_vhdlExport->writeClocksFile("clocks.sdc");
	(*m_vhdlExport)(design.getCircuit());

	//recordVCD("internal.vcd");
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

}
