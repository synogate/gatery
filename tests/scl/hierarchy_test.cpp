
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/Simulator.h>

#include <gatery/scl/stream/strm.h>
#include <gatery/scl/io/SpiMaster.h>

#include <gatery/scl/sim/SimulationSequencer.h>

using namespace boost::unit_test;
using namespace gtry;


BOOST_FIXTURE_TEST_CASE(areaTestForFun, BoostUnitTestSimulationFixture, * boost::unit_test::disabled())
{
	std::cout << "Starting test case..." << std::endl;
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	Area area("firstArea", true);

	using namespace gtry;
	UInt loopNode = 13_b;
	loopNode += 1;
	loopNode += 1;

	UInt firstSignal = 3_b;
	pinIn(firstSignal, "firstSignal");
	{
		Area area("secondArea", true);
		UInt secondSignal = firstSignal + 1;
		HCL_NAMED(secondSignal);
		UInt outputSignal = secondSignal + 1;
		pinOut(outputSignal, "outputSignal");
	}

	design.postprocess();


	runTicks(clk.getClk(), 10);
}

BOOST_FIXTURE_TEST_CASE(loopDetectionTest, BoostUnitTestSimulationFixture, *boost::unit_test::disabled())
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	using namespace gtry;
	UInt  myInt = 13_b;
	myInt += 1;
	myInt += 1;

	design.postprocess();

	runTicks(clk.getClk(), 10);
}
