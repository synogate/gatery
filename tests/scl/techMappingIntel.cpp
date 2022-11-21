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


#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/arch/intel/ALTPLL.h>
#include <gatery/scl/utils/GlobalBuffer.h>
#include <gatery/scl/io/ddr.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canCompileIntel(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL() && gtry::GHDLGlobalFixture::hasIntelLibrary();
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


BOOST_FIXTURE_TEST_CASE(instantiateAltPll, gtry::GHDLTestFixture)
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




BOOST_FIXTURE_TEST_CASE(testAltPll, gtry::GHDLTestFixture)
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


BOOST_FIXTURE_TEST_CASE(instantiate_scl_ddr, gtry::GHDLTestFixture)
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

	Bit d1 = pinIn().setName("d1");
	Bit d2 = pinIn().setName("d2");

	Bit o = scl::ddr(d1, d2);
	
	pinOut(o).setName("ddr_output");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"ALTDDIO_OUT"}));
}


BOOST_FIXTURE_TEST_CASE(instantiate_simulate_scl_ddr, gtry::GHDLTestFixture)
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

	Bit d1 = pinIn().setName("d1");
	Bit d2 = pinIn().setName("d2");
	HCL_NAMED(d1);
	HCL_NAMED(d2);

	Bit o = scl::ddr(d1, d2);

	HCL_NAMED(o);
	
	pinOut(o).setName("ddr_output");

	pinOut(reg(d1)).setName("clockUser");

	addSimulationProcess([=,this]()->SimProcess {
		co_await OnClk(clock1);

		BOOST_TEST(!simu(o).allDefined());

		co_await OnClk(clock1);

		for ([[maybe_unused]] auto i : gtry::utils::Range(20)) {
			bool a = (i & 1);
			bool b = (i & 2);

			simu(d1) = a?'1':'0';
			simu(d2) = b?'1':'0';

			fork([](Seconds cyclePeriod, const Bit &out, bool a, bool b)->SimProcess {
				// Wait for one full clock cycle (because of ddr registers) and then a quarter cycle in case the simulation model has some modeled delay.
				co_await WaitFor(Seconds{5,4} * cyclePeriod);

				BOOST_TEST(simu(out) == (a?'1':'0'));

				// Check the other after half a cycle.
				co_await WaitFor(cyclePeriod/Seconds{2});

				BOOST_TEST(simu(out) == (b?'1':'0'));

			}(Seconds{1} / clock1.absoluteFrequency(), o, a, b));

			co_await OnClk(clock1);
		}

		stopTest();
	});



	runTest(Seconds{100} / clock1.absoluteFrequency());
	BOOST_TEST(exportContains(std::regex{"ALTDDIO_OUT"}));
}





BOOST_FIXTURE_TEST_CASE(instantiate_scl_ddr_for_clock, gtry::GHDLTestFixture)
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

	Bit o = scl::ddr('1', '0');
	
	pinOut(o).setName("ddr_output");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"ALTDDIO_OUT"}));
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




BOOST_AUTO_TEST_SUITE_END()
