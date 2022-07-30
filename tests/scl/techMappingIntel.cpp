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



BOOST_AUTO_TEST_SUITE_END()
