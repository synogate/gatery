/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "scl/pch.h"

#include "MappingTests_IO.h"
#include "MappingTests_Memory.h"

#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/arch/intel/ALTPLL.h>
#include <gatery/scl/utils/GlobalBuffer.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canCompileIntel(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL() && gtry::GHDLGlobalFixture::hasIntelLibrary();
}


namespace {

template<class Fixture>
struct TestWithDefaultDevice : public Fixture
{
	TestWithDefaultDevice() {
		auto device = std::make_unique<gtry::scl::IntelDevice>();
		//device->setupMAX10();
		device->setupArria10();
		Fixture::design.setTargetTechnology(std::move(device));
	}
};

}



BOOST_AUTO_TEST_SUITE(IntelTechMapping, * precondition(canCompileIntel))

BOOST_FIXTURE_TEST_CASE(testGlobalBuffer, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupMAX10();
	design.setTargetTechnology(std::move(device));


	Bit b = pinIn().setName("input");
	b = scl::bufG(b);
	pinOut(b).setName("output");


	testCompilation();
	BOOST_TEST(exportContains(std::regex{"GLOBAL"}));
}


BOOST_FIXTURE_TEST_CASE(SCFifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupMAX10();
	design.setTargetTechnology(std::move(device));

	scl::Fifo<UInt> fifo(128, 8_b);

	Bit inValid = pinIn().setName("inValid");
	UInt inData = pinIn(8_b).setName("inData");
	IF (inValid)
		fifo.push(inData);

	UInt outData = fifo.peek();
	Bit outValid = !fifo.empty();
	IF (outValid)
		fifo.pop();
	pinOut(outData).setName("outData");
	pinOut(outValid).setName("outValid");


	fifo.generate();


	testCompilation();
}

BOOST_FIXTURE_TEST_CASE(DCFifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupMAX10();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	Clock clock2({
			.absoluteFrequency = {{75'000'000,1}},
			//.resetType = Clock::ResetType::ASYNCHRONOUS,
			.initializeRegs = false,
	});
	HCL_NAMED(clock2);

	scl::Fifo<UInt> fifo(128, 8_b);

	{
		ClockScope clkScp(clock1);
		Bit inValid = pinIn().setName("inValid");
		UInt inData = pinIn(8_b).setName("inData");
		IF (inValid)
			fifo.push(inData);
	}

	{
		ClockScope clkScp(clock2);
		UInt outData = fifo.peek();
		Bit outValid = !fifo.empty();
		IF (outValid)
			fifo.pop();
		pinOut(outData).setName("outData");
		pinOut(outValid).setName("outValid");
	}

	fifo.generate();


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(instantiateAltPll, gtry::GHDLTestFixture, * boost::unit_test::disabled())
{
	using namespace gtry;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupMAX10();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	ClockScope scp(clock1);

	auto *pll = design.createNode<scl::arch::intel::ALTPLL>();
	pll->setClock(0, clock1.getClk());

	pll->configureDeviceFamily("MAX 10");
	pll->configureClock(0, 2, 3, 50, 0);


	Bit clkSignal; // Leave unconnected to let the simulator drive the clock signal during simulation
	clkSignal.exportOverride(pll->getOutputBVec(scl::arch::intel::ALTPLL::OUT_CLK)[0]);
	pinOut(clkSignal).setName("clkOut");
	

	Bit rstSignal; // Leave unconnected to let the simulator drive the clock's reset signal during simulation
	rstSignal.exportOverride(!pll->getOutputBit(scl::arch::intel::ALTPLL::OUT_LOCKED) | clock1.rstSignal());
	pinOut(rstSignal).setName("rstOut");
	


	testCompilation();
}




BOOST_FIXTURE_TEST_CASE(testAltPll, gtry::GHDLTestFixture, * boost::unit_test::disabled())
{
	using namespace gtry;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupMAX10();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	Clock clock2({
			.absoluteFrequency = {{75'000'000,1}},
			//.resetType = Clock::ResetType::ASYNCHRONOUS,
			.initializeRegs = false,
	});
	HCL_NAMED(clock2);

	{
		Area area("clockArea", true);

		auto *pll = design.createNode<scl::arch::intel::ALTPLL>();
		pll->setClock(0, clock1.getClk());

		pll->configureDeviceFamily("MAX 10");
		pll->configureClock(0, 2, 3, 50, 0);


		Bit clkSignal; // Leave unconnected to let the simulator drive the clock signal during simulation
		clkSignal.exportOverride(pll->getOutputBVec(scl::arch::intel::ALTPLL::OUT_CLK)[0]);
		HCL_NAMED(clkSignal);
		clock2.overrideClkWith(clkSignal);


		Bit rstSignal; // Leave unconnected to let the simulator drive the clock's reset signal during simulation
		rstSignal.exportOverride(!pll->getOutputBit(scl::arch::intel::ALTPLL::OUT_LOCKED) | clock1.rstSignal());
		HCL_NAMED(rstSignal);
		clock2.overrideRstWith(rstSignal);
	}

	scl::Fifo<UInt> fifo(128, 8_b);

	{
		ClockScope clkScp(clock1);
		Bit inValid = pinIn().setName("inValid");
		UInt inData = pinIn(8_b).setName("inData");
		IF (inValid)
			fifo.push(inData);
	}

	{
		ClockScope clkScp(clock2);
		UInt outData = fifo.peek();
		Bit outValid = !fifo.empty();
		IF (outValid)
			fifo.pop();
		pinOut(outData).setName("outData");
		pinOut(outValid).setName("outValid");
	}

	fifo.generate();


	testCompilation();
}

BOOST_FIXTURE_TEST_CASE(scl_ddr, TestWithDefaultDevice<Test_ODDR>)
{
	execute();
	BOOST_TEST(exportContains(std::regex{"ALTDDIO_OUT"}));
}



BOOST_FIXTURE_TEST_CASE(scl_ddr_for_clock, TestWithDefaultDevice<Test_ODDR_ForClock>)
{
	execute();
	BOOST_TEST(exportContains(std::regex{"ALTDDIO_OUT"}));
}





BOOST_FIXTURE_TEST_CASE(histogram_noAddress, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 1;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
}





BOOST_FIXTURE_TEST_CASE(lutram_1, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
}

BOOST_FIXTURE_TEST_CASE(lutram_2, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 32;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
}

BOOST_FIXTURE_TEST_CASE(blockram_1, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 512;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_2, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 512;
	iterationFactor = 4;
	bucketWidth = 32_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_2_cycle_latency, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 5;
	twoCycleLatencyBRam = true;
	iterationFactor = 4;
	bucketWidth = 64_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}


BOOST_FIXTURE_TEST_CASE(external_high_latency, TestWithDefaultDevice<Test_Histogram>)
{
	using namespace gtry;
	numBuckets = 128;
	iterationFactor = 10;
	bucketWidth = 16_b;
	highLatencyExternal = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"rd_address : OUT STD_LOGIC_VECTOR[\\S\\s]*rd_readdata : IN STD_LOGIC_VECTOR[\\S\\s]*wr_address : OUT STD_LOGIC_VECTOR[\\S\\s]*wr_writedata : OUT STD_LOGIC_VECTOR[\\S\\s]*wr_write"}));
}



BOOST_FIXTURE_TEST_CASE(readOutputBugfix, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			{
				Area area("producingSubArea", true);
				output = input ^ '1';
			}
			{
				Area area("consumingSubArea", true);
				output2 = output ^ '1';
			}
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation(TARGET_QUARTUS);
	BOOST_TEST(exportContains(std::regex{"workaroundEntityInOut08Bug"}));
	BOOST_TEST(exportContains(std::regex{"workaroundReadOut08Bug"}));
	//DesignScope::visualize("test_inout08");
}



BOOST_FIXTURE_TEST_CASE(readOutputLocalBugfix, gtry::GHDLTestFixture)
{
	using namespace gtry;

    {
		Bit input = pinIn().setName("input");
		Bit output;
		Bit output2;
		{
			Area area("mainArea", true);

			output = input ^ '1';
			output2 = output ^ '1';
		}

        pinOut(output).setName("out");
        pinOut(output2).setName("out2");
    }

	testCompilation(TARGET_QUARTUS);
	BOOST_TEST(exportContains(std::regex{"workaroundReadOut08Bug"}));
	//DesignScope::visualize("test_outputLocal");
}





BOOST_FIXTURE_TEST_CASE(sdp_dualclock_small, TestWithDefaultDevice<Test_SDP_DualClock>)
{
//	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	depth = 16;
	elemSize = 8_b;
	numWrites = 10;
	execute();
	// Lutrams only support one clock, so even the small ones should result in the use of blockrams.
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}

BOOST_FIXTURE_TEST_CASE(sdp_dualclock_large, TestWithDefaultDevice<Test_SDP_DualClock>)
{
	//forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	depth = 4096;
	elemSize = 8_b;
	numWrites = 2000;
	execute();
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}


/*
BOOST_FIXTURE_TEST_CASE(readEnable, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	execute();
	BOOST_TEST(exportContains(std::regex{"altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
}
*/

BOOST_FIXTURE_TEST_CASE(readEnable_bram_2Cycle, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	twoCycleLatencyBRam = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"altsyncram"}));
}


BOOST_AUTO_TEST_SUITE_END()
