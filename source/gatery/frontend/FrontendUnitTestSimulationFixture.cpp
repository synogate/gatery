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
#include "FrontendUnitTestSimulationFixture.h"

#include <gatery/simulation/Simulator.h>
#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/hlim/Circuit.h>
#include <boost/test/unit_test.hpp>
#include <gatery/debug/websocks/WebSocksInterface.h>


namespace gtry {

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
	// Force destruct of simulator (and all frontend signals held inside coroutines) before destruction of DesignScope in base class.
	m_simulator.reset(nullptr);
}

void UnitTestSimulationFixture::eval()
{
	prepRun();
	sim::UnitTestSimulationFixture::eval(design.getCircuit());
}

void UnitTestSimulationFixture::runTicks(const hlim::Clock* clock, unsigned numTicks)
{
	prepRun();
	sim::UnitTestSimulationFixture::runTicks(design.getCircuit(), clock, numTicks);
}


void UnitTestSimulationFixture::recordVCD(const std::string& filename)
{
	m_vcdSink.emplace(design.getCircuit(), *m_simulator, filename.c_str());
	m_vcdSink->addAllPins();
	m_vcdSink->addAllNamedSignals();
}

void UnitTestSimulationFixture::outputVHDL(const std::string& filename, bool includeTest)
{
	m_vhdlExport.emplace(filename);
	(*m_vhdlExport)(design.getCircuit());

	if (includeTest) {
		m_vhdlExport->addTestbenchRecorder(*m_simulator, "testbench");
		//m_vhdlExport->writeGHDLScript("runGHDL.sh");
	}
}

void UnitTestSimulationFixture::stopTest()
{
	m_simulator->abort();
	m_stopTestCalled = true;
}

bool UnitTestSimulationFixture::runHitsTimeout(const hlim::ClockRational &timeoutSeconds)
{
	prepRun();
	m_stopTestCalled = false;
	m_simulator->compileProgram(design.getCircuit());
	m_simulator->powerOn();
	m_simulator->advance(timeoutSeconds);

	if (!m_errors.empty())
		BOOST_FAIL(m_errors.front());
	if (!m_warnings.empty())
		BOOST_ERROR(m_warnings.front());

	return !m_stopTestCalled;
}

void UnitTestSimulationFixture::setup()
{
}

void UnitTestSimulationFixture::teardown()
{
}

void UnitTestSimulationFixture::prepRun()
{
}

void BoostUnitTestSimulationFixture::runFixedLengthTest(const hlim::ClockRational &seconds)
{
	runHitsTimeout(seconds);
}

void BoostUnitTestSimulationFixture::runEvalOnlyTest()
{
	eval();
}

void BoostUnitTestSimulationFixture::runTest(const hlim::ClockRational &timeoutSeconds)
{
	BOOST_CHECK_MESSAGE(!runHitsTimeout(timeoutSeconds), "Simulation timed out without being called to a stop by any simulation process!");
}


void BoostUnitTestSimulationFixture::prepRun()
{
	UnitTestSimulationFixture::prepRun();

	const std::string& filename = boost::unit_test::framework::current_test_case().p_name;
	auto& testSuite = boost::unit_test::framework::master_test_suite();
	for (int i = 1; i < testSuite.argc; i++) {
		std::string_view arg(testSuite.argv[i]);

		if (arg == "--vcd")
			recordVCD(filename + ".vcd");
		else if (arg == "--vhdl")
			outputVHDL(filename + ".vhd");
		else if (arg == "--graph-vis" || arg == "--dot")
			design.visualize(filename);
	}
}


void ClockedTest::setup()
{
	BoostUnitTestSimulationFixture::setup();

	m_clock = ClockConfig{
		.absoluteFrequency = 100'000'000,
		.name = "clock",
		.resetType = ClockConfig::ResetType::NONE
	};

	m_clockScope.emplace(*m_clock);
}

void ClockedTest::teardown()
{
	m_clockScope.reset();

	try {
		design.postprocess();
		runTest(m_timeout);
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;

		gtry::dbg::WebSocksInterface::create(1337);
		dbg::awaitDebugger();
		dbg::stopInDebugger();
	}

	m_clock.reset();

	BoostUnitTestSimulationFixture::teardown();
}

}
