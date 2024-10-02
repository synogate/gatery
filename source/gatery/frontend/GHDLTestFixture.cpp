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

#include "GHDLTestFixture.h"

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/scl/synthesisTools/GHDL.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>
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

namespace gtry {

boost::filesystem::path GHDLGlobalFixture::m_ghdlExecutable;
std::filesystem::path GHDLGlobalFixture::m_intelLibrary;
std::filesystem::path GHDLGlobalFixture::m_xilinxLibrary;
std::vector<std::string> GHDLGlobalFixture::m_ghdlArgs;


GHDLGlobalFixture::GHDLGlobalFixture()
{
	m_ghdlExecutable = bp::search_path("ghdl");

	auto& testSuite = boost::unit_test::framework::master_test_suite();
	for (int i = 1; i+1 < testSuite.argc; i++) {
		std::string_view arg(testSuite.argv[i]);

		// /usr/lib/ghdl/vendors/compile-intel.sh --vhdl2008 --all --src /mnt/c/intelFPGA_lite/20.1/quartus/eda/sim_lib/
		if (arg == "--intel")
			m_intelLibrary = testSuite.argv[i+1];
		if (arg == "--xilinx")
			m_xilinxLibrary = testSuite.argv[i+1];
	}

	auto&& libEnv = boost::this_process::wenvironment()[L"GHDL_LIBS_PATH"];
	if (!libEnv.empty())
	{
		for (auto&& dir : std::filesystem::directory_iterator{ libEnv.to_string() })
		{
			if (dir.is_directory())
				m_ghdlArgs.push_back("-P" + dir.path().string());

			// auto detect vivado and quartus libraries for test case filtering
			if(dir.path().filename() == "xilinx-vivado")
				m_xilinxLibrary = dir.path();
			if (dir.path().filename() == "intel")
				m_intelLibrary = dir.path();
		}
	}
}

GHDLGlobalFixture::~GHDLGlobalFixture()
{
}

GHDLTestFixture::GHDLTestFixture()
{
	const auto& testCase = boost::unit_test::framework::current_test_case();
	std::filesystem::path testCaseFile{ std::string{ testCase.p_file_name.begin(), testCase.p_file_name.end() } };
	m_cwd = std::filesystem::path{ "tmp" } / testCaseFile.stem() / testCase.p_name.get();

	if (!std::filesystem::create_directories(m_cwd))
		for (auto&& f : std::filesystem::directory_iterator{ m_cwd })
			if(f.is_regular_file())
				std::filesystem::remove(f);

	m_ghdlArgs = GHDLGlobalFixture::GHDLArgs();
	m_ghdlArgs.push_back("--std=08");
	m_ghdlArgs.push_back("-frelaxed");
	m_ghdlArgs.push_back("--warn-error");

	if (GHDLGlobalFixture::hasIntelLibrary())
		m_ghdlArgs.push_back("-P" + GHDLGlobalFixture::getIntelLibrary().string());

	if (GHDLGlobalFixture::hasXilinxLibrary())
		m_ghdlArgs.push_back("-P" + GHDLGlobalFixture::getXilinxLibrary().string());
}

GHDLTestFixture::~GHDLTestFixture()
{
}

void GHDLTestFixture::addCustomVHDL(std::string name, std::string content)
{
	m_customVhdlFiles[std::move(name)] = std::move(content);
}

void GHDLTestFixture::testCompilation(Flavor flavor)
{
	design.postprocess();

	performVhdlExport(flavor);

	m_generatedSourceFiles = SynthesisTool::sourceFiles(*m_vhdlExport, true, false);
	m_vhdlExport.reset();
	
	boost::filesystem::path ghdlExecutable = bp::search_path("ghdl");

	for (std::filesystem::path& vhdlFile : m_generatedSourceFiles)
		BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, vhdlFile.string()) == 0);

//	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, "design.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-e", "--ieee=synopsys", m_ghdlArgs, "top") == 0);
}

void GHDLTestFixture::prepRun()
{
	design.postprocess();
	BoostUnitTestSimulationFixture::prepRun();

	performVhdlExport();
}

void GHDLTestFixture::performVhdlExport(Flavor flavor)
{
	m_vhdlExport.emplace(m_cwd / "design.vhd");
	for (const auto &file_content : m_customVhdlFiles)
		m_vhdlExport->addCustomVhdlFile(file_content.first, file_content.second);

	m_vhdlExport->outputMode(vhdlOutputMode);
	m_vhdlExport->addTestbenchRecorder(getSimulator(), "testbench", false);
	m_vhdlExport->targetSynthesisTool(new GHDL());
	m_vhdlExport->writeStandAloneProjectFile("compile.sh");

	gtry::SynthesisTool* synthesisTool = nullptr;
	switch (flavor) {
		case TARGET_GHDL:  synthesisTool = new GHDL(); break;
		case TARGET_QUARTUS : synthesisTool = new IntelQuartus(); break;
	}
	m_vhdlExport->targetSynthesisTool(synthesisTool);
	(*m_vhdlExport)(design.getCircuit());
}

void GHDLTestFixture::runTest(const hlim::ClockRational &timeoutSeconds)
{
	m_stopTestCalled = false;
	BoostUnitTestSimulationFixture::runTest(timeoutSeconds);
	BOOST_CHECK_MESSAGE(m_stopTestCalled, "Simulation timed out without being called to a stop by any simulation process!");

	m_generatedSourceFiles = SynthesisTool::sourceFiles(*m_vhdlExport, true, true);
	m_vhdlExport.reset();

	boost::filesystem::path ghdlExecutable = bp::search_path("ghdl");

	for (std::filesystem::path& vhdlFile : m_generatedSourceFiles)
		BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, vhdlFile.string()) == 0);

	// BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, "design.vhd") == 0);
	// BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-a", "--ieee=synopsys", m_ghdlArgs, "testbench.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-e", "--ieee=synopsys", m_ghdlArgs, "testbench") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, bp::start_dir(m_cwd.string()), "-r", "-fsynopsys", m_ghdlArgs, "testbench", "--ieee-asserts=disable", "--vcd=ghdl.vcd", "--assert-level=error") == 0);
}

bool GHDLTestFixture::exportContains(const std::regex &regex)
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
