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
#include <gatery/hlim/Circuit.h>
#include <boost/test/unit_test.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>

namespace bp = boost::process;

namespace gtry {

boost::filesystem::path GHDLGlobalFixture::m_ghdlExecutable;
std::filesystem::path GHDLGlobalFixture::m_intelLibrary;
std::filesystem::path GHDLGlobalFixture::m_xilinxLibrary;


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

}

GHDLGlobalFixture::~GHDLGlobalFixture()
{
}



GHDLTestFixture::GHDLTestFixture()
{
	m_cwd = std::filesystem::current_path();

	std::filesystem::path tmpDir("tmp/");
	std::error_code ignored;
	std::filesystem::remove_all(tmpDir, ignored);
	std::filesystem::create_directories(tmpDir);
    std::filesystem::current_path(tmpDir);
}

GHDLTestFixture::~GHDLTestFixture()
{
	std::filesystem::current_path(m_cwd);
	//std::filesystem::remove_all("tmp/");
}

void GHDLTestFixture::testCompilation(Flavor flavor)
{
	design.postprocess();


	m_vhdlExport.emplace("design.vhd");
	switch (flavor) {
		case TARGET_GHDL : m_vhdlExport->targetSynthesisTool(new GHDL()); break;
		case TARGET_QUARTUS : m_vhdlExport->targetSynthesisTool(new IntelQuartus()); break;
	}
	m_vhdlExport->writeStandAloneProjectFile("compile.sh");
	(*m_vhdlExport)(design.getCircuit());

	m_vhdlExport.reset();
	
	boost::filesystem::path ghdlExecutable = bp::search_path("ghdl");


	if (GHDLGlobalFixture::hasIntelLibrary())
		softlinkAll(GHDLGlobalFixture::getIntelLibrary());

	if (GHDLGlobalFixture::hasXilinxLibrary())
		softlinkAll(GHDLGlobalFixture::getXilinxLibrary());

	BOOST_CHECK(bp::system(ghdlExecutable, "-a", "--std=08", "--ieee=synopsys", "-frelaxed", "--warn-error", "design.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, "-e", "--std=08", "--ieee=synopsys", "-frelaxed", "--warn-error", "top") == 0);
}

void GHDLTestFixture::softlinkAll(const std::filesystem::path &src)
{
	for (const auto &entry : std::filesystem::directory_iterator{src}) {
		if (entry.is_directory() && entry.path().filename() != "." && entry.path().filename() != "..") 
			std::filesystem::create_directory_symlink(entry.path(), entry.path().filename());
	}
}

void GHDLTestFixture::prepRun()
{
	design.postprocess();

	m_vhdlExport.emplace("design.vhd");
	m_vhdlExport->addTestbenchRecorder(getSimulator(), "testbench", true);
	m_vhdlExport->targetSynthesisTool(new GHDL());
	m_vhdlExport->writeStandAloneProjectFile("compile.sh");
	(*m_vhdlExport)(design.getCircuit());

	recordVCD("internal.vcd");
}

void GHDLTestFixture::runTest(const hlim::ClockRational &timeoutSeconds)
{
	m_stopTestCalled = false;
	BoostUnitTestSimulationFixture::runTest(timeoutSeconds);
	BOOST_CHECK_MESSAGE(m_stopTestCalled, "Simulation timed out without being called to a stop by any simulation process!");

	m_vhdlExport.reset();

	boost::filesystem::path ghdlExecutable = bp::search_path("ghdl");

	if (GHDLGlobalFixture::hasIntelLibrary())
		softlinkAll(GHDLGlobalFixture::getIntelLibrary());

	if (GHDLGlobalFixture::hasXilinxLibrary())
		softlinkAll(GHDLGlobalFixture::getXilinxLibrary());

	BOOST_CHECK(bp::system(ghdlExecutable, "-a", "--std=08", "--ieee=synopsys", "-frelaxed", "--warn-error", "design.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, "-a", "--std=08", "--ieee=synopsys", "-frelaxed", "--warn-error", "testbench.vhd") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, "-e", "--std=08", "--ieee=synopsys", "-frelaxed", "--warn-error", "testbench") == 0);
	BOOST_CHECK(bp::system(ghdlExecutable, "-r", "--std=08", "-frelaxed", "-fsynopsys", "testbench", "--ieee-asserts=disable", "--vcd=ghdl.vcd", "--assert-level=error") == 0);
}

bool GHDLTestFixture::exportContains(const std::regex &regex)
{
	std::fstream file("design.vhd", std::fstream::in);
	std::stringstream buffer;
	buffer << file.rdbuf();

	return std::regex_search(buffer.str(), regex);
}

}
