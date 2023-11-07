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
#include <gatery/scl/arch/xilinx/XilinxDevice.h>
#include <gatery/scl/utils/GlobalBuffer.h>

#include <gatery/scl/arch/xilinx/IOBUF.h>
#include <gatery/scl/arch/xilinx/URAM288.h>

#include <gatery/hlim/coreNodes/Node_MultiDriver.h>


#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canCompileXilinx(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL() && gtry::GHDLGlobalFixture::hasXilinxLibrary();
}

namespace {

template<class Fixture>
struct TestWithDefaultDevice : public Fixture
{
	TestWithDefaultDevice() {
		auto device = std::make_unique<gtry::scl::XilinxDevice>();
		//device->setupZynq7();
		device->setupVirtexUltrascale();
		Fixture::design.setTargetTechnology(std::move(device));
	}
};

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


BOOST_FIXTURE_TEST_CASE(scl_ddr, TestWithDefaultDevice<Test_ODDR>)
{
	execute();
	BOOST_TEST(exportContains(std::regex{"ODDR"}));
}

BOOST_FIXTURE_TEST_CASE(scl_ddr_for_clock, TestWithDefaultDevice<Test_ODDR_ForClock>)
{
	execute();
	BOOST_TEST(exportContains(std::regex{"ODDR"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_1, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: activate initialization for lutrams (after proper testing)
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 4;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAM64M8"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_2, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: activate initialization for lutrams (after proper testing)
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 32;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAM256X1D"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_1, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 512;
	bucketWidth = 32_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAMB18E2"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_2, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 512;
	iterationFactor = 4;
	bucketWidth = 64_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAMB36E2"}));
}

BOOST_FIXTURE_TEST_CASE(blockram_cascade, TestWithDefaultDevice<Test_MemoryCascade>)
{
	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	depth = 1 << 16;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAMB36E2"}));
	BOOST_TEST(exportContains(std::regex{"ARCHITECTURE impl OF memory_split_at_addrbit_15"}));
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


BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });
	multiDriver->setName("bidir_signal");

	auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
	pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");


	auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
	pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");


	multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
	multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());


	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));



	testCompilation();
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC;"}));
	//DesignScope::visualize("test_bidir_intra_connection");
}



BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });
	multiDriver->setName("bidir_signal");

	auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
	pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

	multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

	{
		Area area("test", true);
		auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
		pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}


	testCompilation();

	BOOST_TEST(exportContains(std::regex{"in_bidir_signal : INOUT STD_LOGIC;"}));
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC;"}));
	//DesignScope::visualize("test_bidir_intra_connection_different_entities");
}

BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities2, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });
	multiDriver->setName("bidir_signal");

	{
		Area area("test1", true);
		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

		multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}

	{
		Area area("test2", true);
		auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
		pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}


	testCompilation();

	BOOST_TEST(exportContains(std::regex{"in_bidir_signal : INOUT STD_LOGIC;"}));
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC;"}));

	//DesignScope::visualize("test_bidir_intra_connection_different_entities2");
}

BOOST_FIXTURE_TEST_CASE(uram288_cascade, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;

	std::array<scl::arch::xilinx::URAM288, 8> ram;
	for(size_t i = 1; i < ram.size(); ++i)
		ram[i].cascade(ram[i - 1]);

	ram[3].cascadeReg(true);

	for(auto &r : ram)
	{
		r.clock(ClockScope::getClk());
		r.enableOutputRegister(scl::arch::xilinx::URAM288::A, true);
		r.enableOutputRegister(scl::arch::xilinx::URAM288::B, true);
	}

	scl::arch::xilinx::URAM288::PortIn in;
	pinIn(in, "in_a");
	ram.front().port(scl::arch::xilinx::URAM288::A, reg(in));
	pinOut(reg(ram.back().port(scl::arch::xilinx::URAM288::A)), "out_a");

	testCompilation();
}

BOOST_FIXTURE_TEST_CASE(test_bidir_pin_extnode, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	{
		Area area("test1", true);

		auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });

		Bit t = pinIn().setName("T1");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

		multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

		multiDriver->rewireInput(1, Bit(bidirPin(Bit(SignalReadPort(multiDriver)))).readPort());
	}

/*
	{
		Area area("test2", true);

		Bit t = pinIn().setName("T2");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, bidirPin(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O)));
	}
*/

	{
		Area area("test3", true);

		auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });

		Bit t = pinIn().setName("T3");
		Bit i = pinIn().setName("I3");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, i);
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);


		Bit bufOut = iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O);
		multiDriver->rewireInput(0, bufOut.readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

		Bit biPinIn = i;
		biPinIn.exportOverride(Bit(SignalReadPort(multiDriver)));
		Bit biPinOut = tristatePin(biPinIn, t).setName("biPin_3");

		multiDriver->rewireInput(1, biPinOut.readPort());

		Bit o = biPinOut;
		o.exportOverride(Bit(SignalReadPort(multiDriver)));

		pinOut(o).setName("O3");
	}	

	testCompilation();

	//DesignScope::visualize("test_bidir_pin_extnode");
}



BOOST_AUTO_TEST_SUITE_END()
