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
#include <gatery/frontend/EventStatistics.h>

#include <gatery/debug/DebugInterface.h>


namespace gtry {

UnitTestSimulationFixture::UnitTestSimulationFixture()
{
	// Reporting must be setup before anything happens that could be reported on.

	const std::string& filename = boost::unit_test::framework::current_test_case().p_name;
	auto& testSuite = boost::unit_test::framework::master_test_suite();
	for (int i = 1; i < testSuite.argc; i++) {
		std::string_view arg(testSuite.argv[i]);

		if (arg == "--report") {
			if (i+1 >= testSuite.argc)
				throw std::runtime_error("Missing command line argument after --report"); 
			i++;
			std::string_view param(testSuite.argv[i]);
			if (param == "html")
				gtry::dbg::logHtml(filename + "_log/");
			else if (param == "web") {
				gtry::dbg::logWebsocks(1337);
			} else
				throw std::runtime_error("Invalid command line argument after --report");
		}
	}
	gtry::dbg::awaitDebugger();
}

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
	gtry::dbg::stopInDebugger();

	if (m_vhdlExport)
		m_vhdlExport->clearTestbenchRecorder();

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

	if (m_statisticsCounterCsvFile)
		EventStatistics::writeStatTable(*m_statisticsCounterCsvFile);
}


void UnitTestSimulationFixture::recordVCD(const std::string& filename, bool includeMemories)
{
	m_vcdSink.emplace(design.getCircuit(), *m_simulator, filename.c_str());
	m_vcdSink->addAllPins();
	m_vcdSink->addAllNamedSignals();
	m_vcdSink->addAllTaps();

	if(includeMemories)
		m_vcdSink->addAllMemories();
}

void UnitTestSimulationFixture::outputVHDL(const std::string& filename, bool includeTest)
{
	m_vhdlExport.emplace(filename);
	if (includeTest) {
		m_vhdlExport->addTestbenchRecorder(*m_simulator, filename + "_tb", true); 
		//m_vhdlExport->writeGHDLScript("runGHDL.sh");
	}

	(*m_vhdlExport)(design.getCircuit());
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
	m_simulator->commitState();

	if (!m_errors.empty())
		BOOST_FAIL(m_errors.front());
	if (!m_warnings.empty())
		BOOST_ERROR(m_warnings.front());

	if (m_statisticsCounterCsvFile)
		EventStatistics::writeStatTable(*m_statisticsCounterCsvFile);

	return !m_stopTestCalled;
}

size_t UnitTestSimulationFixture::countNodes(const std::function<bool(const hlim::BaseNode*)> &nodeSelector) const
{
	size_t count = 0;
	for (const auto &n : design.getCircuit().getNodes()) {
		if (nodeSelector(n.get()))
			count++;
	}
	return count;
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
		else if(arg == "--vcdm")
			recordVCD(filename + "_with_memories" ".vcd", true);
		else if (arg == "--vhdl")
			outputVHDL(filename + ".vhd");
		else if (arg == "--csv")
			m_statisticsCounterCsvFile = filename + ".csv";
		else if (arg == "--graph-vis" || arg == "--dot")
			design.visualize(filename);
	}
}

ClockedTest::ClockedTest()
{
	m_clock.emplace(ClockConfig{
			.absoluteFrequency = {{100'000'000,1}},
			.initializeRegs = false,
	});
	m_clock->setName("clock");
	m_clockScope.emplace(*m_clock);
}

void ClockedTest::teardown()
{
	design.postprocess();
	//design.getCircuit().postprocess(hlim::MinimalPostprocessing{});
	runTest(m_timeout);

	BoostUnitTestSimulationFixture::teardown();
}

}
