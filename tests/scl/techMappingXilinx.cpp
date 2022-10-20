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
#include <gatery/scl/arch/xilinx/XilinxDevice.h>
#include <gatery/scl/arch/xilinx/ODDR.h>
#include <gatery/scl/utils/GlobalBuffer.h>
#include <gatery/scl/io/ddr.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canCompileXilinx(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL() && gtry::GHDLGlobalFixture::hasXilinxLibrary();
}


BOOST_AUTO_TEST_SUITE(XilinxTechMapping, * precondition(canCompileXilinx))

BOOST_FIXTURE_TEST_CASE(testGlobalBuffer, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	Bit b = pinIn().setName("input");
	b = scl::bufG(b);
	pinOut(b).setName("output");


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(SCFifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
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

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
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


BOOST_FIXTURE_TEST_CASE(instantiateODDR, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	ClockScope scp(clock1);

	auto *ddr = design.createNode<scl::arch::xilinx::ODDR>();
	ddr->attachClock(clock1.getClk(), scl::arch::xilinx::ODDR::CLK_IN);

	ddr->setEdgeMode(scl::arch::xilinx::ODDR::SAME_EDGE);
	ddr->setInitialOutputValue(false);

	ddr->setInput(scl::arch::xilinx::ODDR::IN_D1, pinIn().setName("d1"));
	ddr->setInput(scl::arch::xilinx::ODDR::IN_D2, pinIn().setName("d2"));
	ddr->setInput(scl::arch::xilinx::ODDR::IN_SET, clock1.rstSignal());
	ddr->setInput(scl::arch::xilinx::ODDR::IN_CE, Bit('1'));
	
	pinOut(ddr->getOutputBit(scl::arch::xilinx::ODDR::OUT_Q)).setName("ddr_output");


	testCompilation();
}

BOOST_FIXTURE_TEST_CASE(instantiate_scl_ddr, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
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
}


BOOST_AUTO_TEST_SUITE_END()
