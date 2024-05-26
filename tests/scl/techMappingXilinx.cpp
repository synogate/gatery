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
#include <gatery/scl/arch/xilinx/UltraRAM.h>
#include <gatery/scl/arch/xilinx/DSP48E2.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/math/PipelinedMath.h>

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






BOOST_FIXTURE_TEST_CASE(histogram_noAddress, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: activate initialization for lutrams (after proper testing)
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 1;
	bucketWidth = 8_b;
	execute();
	BOOST_TEST(exportContains(std::regex{"TYPE mem_type IS array"}));
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
	BOOST_TEST(exportContains(std::regex{"RAM64M8"}));
}

BOOST_FIXTURE_TEST_CASE(lutram_3, TestWithDefaultDevice<Test_Histogram>)
{
	forceNoInitialization = true; // todo: activate initialization for lutrams (after proper testing)
	forceMemoryResetLogic = true;

	using namespace gtry;
	numBuckets = 256;
	bucketWidth = 4_b;
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
	BOOST_TEST(exportContains(std::regex{"RAMB36E2_inst : UNISIM.VCOMPONENTS.RAMB36E2"}));
	BOOST_TEST(exportContains(std::regex{"RAMB36E2_inst_2 : UNISIM.VCOMPONENTS.RAMB36E2"}));
	BOOST_TEST(exportContains(std::regex{"ARCHITECTURE impl OF memory_split"}));
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

	BOOST_TEST(exportContains(std::regex{"in_bidir_signal : INOUT STD_LOGIC"}));
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC"}));
	//DesignScope::visualize("test_bidir_intra_connection_different_entities");
}



BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities2, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));



	auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
	pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

	{
		Area area("test", true);
		auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .type = hlim::ConnectionType::BOOL, .width = 1 });
		multiDriver->setName("bidir_signal");
		multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

		auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
		pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}


	testCompilation();

	BOOST_TEST(exportContains(std::regex{"in_bidir_signal : INOUT STD_LOGIC"}));
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC"}));
	//DesignScope::visualize("test_bidir_intra_connection_different_entities2");
}

BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities3, gtry::GHDLTestFixture)
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

	BOOST_TEST(exportContains(std::regex{"in_bidir_signal : INOUT STD_LOGIC"}));
	BOOST_TEST(exportContains(std::regex{"SIGNAL s_bidir_signal : STD_LOGIC"}));

	//DesignScope::visualize("test_bidir_intra_connection_different_entities3");
}

BOOST_FIXTURE_TEST_CASE(uram288_cascade, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;

	std::array<scl::arch::xilinx::URAM288, 8> ram;
	for(size_t i = 1; i < ram.size(); ++i)
		ram[i].cascade(ram[i - 1], ram.size());

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

BOOST_FIXTURE_TEST_CASE(ultraRamHelper, TestWithDefaultDevice<gtry::GHDLTestFixture>, * boost::unit_test::disabled())
{
	using namespace gtry;
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::array ram = gtry::scl::arch::xilinx::ultraRam(4096*8, { .name = "testRam", .aSourceW = 1_b, .bSourceW = 1_b });

	std::array<scl::TileLinkMasterModel, 2> m;
	for (size_t i = 0; i < m.size(); ++i)
	{
		m[i].init("m" + std::to_string(i), ram[i].a->address.width(), 64_b, 2_b, 1_b);
		(scl::TileLinkUB&)ram[i] <<= regDecouple(move(m[i].getLink()));
	}

	testCompilation();

	addSimulationProcess([&]()->SimProcess {
		co_await OnClk(clock);

		{ // write conflict
			fork(m[0].put(8, 3, 0x1234, clock));
			fork(m[1].put(8, 3, 0xABCD, clock));
			auto [val, def, err] = co_await m[1].get(8, 3, clock);
			BOOST_TEST(val == 0xabcd);
		}

		{ // write before read
			fork(m[0].put(8, 3, 0x1234, clock));
			auto [val, def, err] = co_await m[1].get(8, 3, clock);
			BOOST_TEST(val == 0x1234);
		}

		{ // read before write
			fork(m[1].put(8, 3, 0xABCD, clock));
			auto [val, def, err] = co_await m[0].get(8, 3, clock);
			BOOST_TEST(val == 0x1234);
		}

		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		stopTest();
	});

	runTest({ 1,1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(mulAccumulate, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;
	Clock clock(ClockConfig{ .absoluteFrequency = 100'000'000, .resetName = "reset", .triggerEvent = ClockConfig::TriggerEvent::RISING, .resetActive = ClockConfig::ResetActive::HIGH });
	ClockScope clkScp(clock);

	SInt a = (SInt)pinIn(18_b).setName("a");
	SInt b = (SInt)pinIn(18_b).setName("b");
	Bit restart = pinIn().setName("restart");
	Bit valid = pinIn().setName("valid");
	SInt p = scl::arch::xilinx::mulAccumulate(a, b, restart, valid);
	pinOut(p, "p");

	addSimulationProcess([&]()->SimProcess {

		simu(a) = 0;
		simu(b) = 0;
		simu(restart) = '1';
		simu(valid) = '1';
		co_await OnClk(clock);

		simu(a) = 1;
		simu(b) = 1;
		simu(restart) = '0';
		for(size_t i = 0; i < 4; ++i)
		{
			simu(valid) = i % 2 == 1;
			co_await OnClk(clock);
		}

		BOOST_TEST(simu(p) == 0);
		co_await OnClk(clock);
		BOOST_TEST(simu(p) == 1);

		simu(a) = -3;
		simu(b) = 4;
		simu(restart) = '1';
		co_await OnClk(clock);
		BOOST_TEST(simu(p) == 1);
		simu(a) = 5;
		simu(b) = -1;
		simu(restart) = '0';

		co_await OnClk(clock);
		BOOST_TEST(simu(p) == 2);

		co_await OnClk(clock);
		BOOST_TEST(simu(p) == 3);

		co_await OnClk(clock);
		BOOST_TEST(simu(p) == -12);

		co_await OnClk(clock);
		BOOST_TEST(simu(p) == -17);

		co_await OnClk(clock);
		stopTest();
	});

	runTest({ 1,1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(mulAccumulate2, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;
	Clock clock(ClockConfig{ .absoluteFrequency = 100'000'000, .resetName = "reset", .triggerEvent = ClockConfig::TriggerEvent::RISING, .resetActive = ClockConfig::ResetActive::HIGH });
	ClockScope clkScp(clock);

	SInt a1 = (SInt)pinIn(18_b).setName("a1");
	SInt b1 = (SInt)pinIn(18_b).setName("b1");
	SInt a2 = (SInt)pinIn(18_b).setName("a2");
	SInt b2 = (SInt)pinIn(18_b).setName("b2");
	Bit restart = pinIn().setName("restart");
	Bit valid = pinIn().setName("valid");

	SInt p = scl::arch::xilinx::mulAccumulate(a1, b1, a2, b2, restart, valid);
	pinOut(p, "p");

	addSimulationProcess([&]()->SimProcess {
		simu(a1) = 0;
		simu(b1) = 0;
		simu(a2) = 0;
		simu(b2) = 0;
		simu(restart) = '1';
		simu(valid) = '1';
		co_await OnClk(clock);

		simu(a1) = 1;
		simu(b1) = 1;
		simu(restart) = '0';
		for (size_t i = 0; i < 4; ++i)
		{
			simu(valid) = i % 2 == 1;
			co_await OnClk(clock);
		}

		simu(a2) = -5;
		simu(b2) = -9;
		co_await OnClk(clock);

		simu(a1) = -3;
		simu(b1) = 4;
		simu(restart) = '1';
		co_await OnClk(clock);

		simu(a1) = 5;
		simu(b1) = -1;
		simu(restart) = '0';
	});

	addSimulationProcess([&]()->SimProcess {
		for(size_t i = 0; i < 6; ++i)
			co_await OnClk(clock);

		for (int expected : {0, 1, 1, 2, 3 + 45, -12 + 45, -17 + 2*45})
		{
			BOOST_TEST(simu(p) == expected);
			co_await OnClk(clock);
		}
		
		stopTest();
	});

	runTest({ 1,1'000'000 });
}

BOOST_FIXTURE_TEST_CASE(mulAccumulate_fuzz, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;
	Clock clock(ClockConfig{ .absoluteFrequency = 100'000'000, .resetName = "reset", .triggerEvent = ClockConfig::TriggerEvent::RISING, .resetActive = ClockConfig::ResetActive::HIGH });
	ClockScope clkScp(clock);

	SInt a1 = (SInt)pinIn(27_b).setName("a1");
	SInt b1 = (SInt)pinIn(18_b).setName("b1");
	SInt a2 = (SInt)pinIn(27_b).setName("a2");
	SInt b2 = (SInt)pinIn(18_b).setName("b2");
	Bit restart = pinIn().setName("restart");
	Bit valid = pinIn().setName("valid");

	SInt p = scl::arch::xilinx::mulAccumulate(a1, b1, a2, b2, restart, valid);
	pinOut(p, "p");

	struct FuzzData {
		std::int64_t a1, a2, b1, b2;
		bool restart;
	};

	std::mt19937 rng;
	std::uniform_int_distribution<int> rngBig(-(1 << 26), (1 << 26)-1);
	std::uniform_int_distribution<int> rngSmall(-(1 << 17), (1 << 17)-1);
	std::bernoulli_distribution res(0.7f);

	std::vector<FuzzData> fuzzData;
	for (size_t i = 0; i < 100; i++)
		fuzzData.push_back(FuzzData{
			.a1 = rngBig(rng),
			.a2 = rngBig(rng),
			.b1 = rngSmall(rng),
			.b2 = rngSmall(rng),
			.restart = res(rng) || i == 0
		});

	addSimulationProcess([&]()->SimProcess {
		simu(a1) = 0;
		simu(b1) = 0;
		simu(a2) = 0;
		simu(b2) = 0;
		simu(restart) = '1';
		simu(valid) = '1';
		co_await OnClk(clock);

		for (const auto &data : fuzzData) {
			simu(a1) = data.a1;
			simu(b1) = data.b1;
			simu(a2) = data.a2;
			simu(b2) = data.b2;
			simu(restart) = data.restart;
			simu(valid) = '1';
			co_await OnClk(clock);
		}
		simu(valid) = '0';
	});

	addSimulationProcess([&]()->SimProcess {
		for(size_t i = 0; i < 6; ++i)
			co_await OnClk(clock);

		std::int64_t expected = 0;
		for (const auto &data : fuzzData) {
			if (data.restart)
				expected = 0;
			expected += data.a1 * data.b1 + data.a2 * data.b2;
			BOOST_TEST(simu(p) == expected);
			co_await OnClk(clock);
		}
		
		stopTest();
	});

	runTest({ 500,100'000'000 });
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_double_clb_test, TestWithDefaultDevice<gtry::GHDLTestFixture>)
{
	using namespace gtry;
	Clock clock({ .absoluteFrequency = 100'000'000, .name = "clk" });
	Clock clockFast = clock.deriveClock({ .frequencyMultiplier = 2, .name = "clk2x" });
	ClockScope clkScp(clock);

	Vector<std::tuple<SInt, SInt, Bit, Bit>> in(2);
	for(auto& it : in)
	{
		get<0>(it) = 18_b;
		get<1>(it) = 18_b;
	}
	pinIn(in, "in");

	Vector<SInt> out = scl::doublePump<SInt, std::tuple<SInt, SInt, Bit, Bit>>([](const std::tuple<SInt, SInt, Bit, Bit>& params) {
		return gtry::scl::arch::xilinx::mulAccumulate(
			get<0>(params), 
			get<1>(params),
			get<2>(params),
			get<3>(params)
		);
	}, in, clockFast);
	pinOut(out, "out");

	addSimulationProcess([&]()->SimProcess {
		simu(get<0>(in[0])) = 1;
		simu(get<1>(in[0])) = 3;
		simu(get<2>(in[0])) = '1';
		simu(get<3>(in[0])) = '1';
		simu(get<0>(in[1])) = 5;
		simu(get<1>(in[1])) = 7;
		simu(get<2>(in[1])) = '0';
		simu(get<3>(in[1])) = '1';

		co_await OnClk(clock);
		simu(get<2>(in[0])) = '0';

		for (size_t i = 0; i < 3; ++i)
			co_await OnClk(clock);

		for(int i = 0; i < 8; ++i)
		{
			BOOST_TEST(simu(out[1]) == 38 * (i + 1));
			co_await OnClk(clock);
		}

		stopTest();
	});

	runTest({ 1,1'000'000 });
}

class DSP48E2_mul_fixture : public TestWithDefaultDevice<gtry::GHDLTestFixture>
{
protected:
	void test(gtry::BitWidth aW, gtry::BitWidth bW, gtry::BitWidth resultW, size_t resultOffset)
	{
		using namespace gtry;
		Clock clock({ .absoluteFrequency = 100'000'000, .name = "clk" });
		ClockScope clkScp(clock);

		UInt a = pinIn(aW).setName("a");
		UInt b = pinIn(bW).setName("b");

		auto [c, latency] = scl::arch::xilinx::mul(a, b, resultW, resultOffset);
		pinOut(c, "c");

		UInt e = pinIn(c.width()).setName("e");

		std::queue<uint64_t> expected;

		addSimulationProcess([&]()->SimProcess {
			std::mt19937_64 rng{ std::random_device{}() };

			for (size_t i = 0; i < 64; ++i)
			{
				uint64_t aVal = rng() & a.width().mask();
				simu(a) = aVal;
				uint64_t bVal = rng() & b.width().mask();
				simu(b) = bVal;
				expected.push((aVal * bVal >> resultOffset) & resultW.mask());
				co_await OnClk(clock);
			}
		});

		addSimulationProcess([&]()->SimProcess {
			for(size_t i = 0; i < latency; ++i)
				co_await OnClk(clock);
			while (!expected.empty())
			{
				simu(e) = expected.front();
				co_await OnClk(clock);
				BOOST_TEST(simu(c) == expected.front());
				expected.pop();
			}
			BOOST_TEST(simu(c) != 0);
			stopTest();
		});

		runTest({ 2, 1'000'000 });
	}
};

BOOST_FIXTURE_TEST_CASE(DSP48E2_mul_symetric_full_test, DSP48E2_mul_fixture)
{
	using namespace gtry;
	test(32_b, 32_b, 64_b, 0);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_mul_asymetric_full_test, DSP48E2_mul_fixture)
{
	using namespace gtry;
	test(26_b, 38_b, 64_b, 0);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_mul_symetric_partial_test, DSP48E2_mul_fixture)
{
	using namespace gtry;
	test(48_b + 13_b, 48_b + 13_b, 48_b, 13);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_mul_asymetric_partial_test, DSP48E2_mul_fixture)
{
	using namespace gtry;
	test(26_b, 38_b, 16_b, 20);
}


class DSP48E2_pipelinedMul_fixture : public TestWithDefaultDevice<gtry::GHDLTestFixture>
{
protected:
	void test(gtry::BitWidth aW, gtry::BitWidth bW, gtry::BitWidth resultW, size_t resultOffset)
	{
		using namespace gtry;
		Clock clock({ .absoluteFrequency = 100'000'000, .name = "clk" });
		ClockScope clkScp(clock);

		PipeBalanceGroup group;

		UInt a = pinIn(aW).setName("a");
		UInt b = pinIn(bW).setName("b");

		UInt retimeableA = a;
		UInt retimeableB = b;
		retimeableA = group(retimeableA);
		retimeableB = group(retimeableB);

		auto c = scl::math::pipelinedMul(retimeableA, retimeableB, resultW, resultOffset);
		pinOut(c, "c");

		UInt e = pinIn(c.width()).setName("e");

		std::queue<uint64_t> expected;

		addSimulationProcess([&]()->SimProcess {
			std::mt19937_64 rng{ std::random_device{}() };

			for (size_t i = 0; i < 64; ++i)
			{
				uint64_t aVal = rng() & a.width().mask();
				simu(a) = aVal;
				uint64_t bVal = rng() & b.width().mask();
				simu(b) = bVal;
				expected.push((aVal * bVal >> resultOffset) & resultW.mask());
				co_await OnClk(clock);
			}
		});

		addSimulationProcess([&]()->SimProcess {
			size_t latency = group.getNumPipeBalanceGroupStages();
			for(size_t i = 0; i < latency; ++i)
				co_await OnClk(clock);
			while (!expected.empty())
			{
				simu(e) = expected.front();
				co_await OnClk(clock);
				BOOST_TEST(simu(c) == expected.front());
				expected.pop();
			}
			BOOST_TEST(simu(c) != 0);
			stopTest();
		});

		design.visualize("before");
		runTest({ 2, 1'000'000 });
		design.visualize("after");

		BOOST_TEST(exportContains(std::regex{"DSP48E2"}));
	}
};

BOOST_FIXTURE_TEST_CASE(DSP48E2_pipelinedMul_symetric_full_test, DSP48E2_pipelinedMul_fixture)
{
	using namespace gtry;
	test(8_b, 8_b, 16_b, 0);
	//test(32_b, 32_b, 64_b, 0);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_pipelinedMul_asymetric_full_test, DSP48E2_pipelinedMul_fixture)
{
	using namespace gtry;
	test(26_b, 38_b, 64_b, 0);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_pipelinedMul_symetric_partial_test, DSP48E2_pipelinedMul_fixture)
{
	using namespace gtry;
	test(48_b + 13_b, 48_b + 13_b, 48_b, 13);
}

BOOST_FIXTURE_TEST_CASE(DSP48E2_pipelinedMul_asymetric_partial_test, DSP48E2_pipelinedMul_fixture)
{
	using namespace gtry;
	test(26_b, 38_b, 16_b, 20);
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


BOOST_FIXTURE_TEST_CASE(sdp_dualclock_small, TestWithDefaultDevice<Test_SDP_DualClock>)
{
	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	depth = 16;
	elemSize = 8_b;
	numWrites = 10;
	execute();
	// Lutrams only support one clock, so even the small ones should result in the use of blockrams.
	BOOST_TEST(exportContains(std::regex{"RAMB18E2_inst : UNISIM.VCOMPONENTS.RAMB18E2"}));
}

BOOST_FIXTURE_TEST_CASE(sdp_dualclock_large, TestWithDefaultDevice<Test_SDP_DualClock>)
{
	forceNoInitialization = true; // todo: implement initialization for blockrams
	forceMemoryResetLogic = true;

	using namespace gtry;
	depth = 4096;
	elemSize = 8_b;
	numWrites = 2000;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAMB36E2_inst : UNISIM.VCOMPONENTS.RAMB36E2"}));
}


/*
BOOST_FIXTURE_TEST_CASE(readEnable, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAM64M8"}));
}
*/

BOOST_FIXTURE_TEST_CASE(readEnable_bram_2Cycle, TestWithDefaultDevice<Test_ReadEnable>)
{
	using namespace gtry;
	twoCycleLatencyBRam = true;
	execute();
	BOOST_TEST(exportContains(std::regex{"RAMB18E2_inst : UNISIM.VCOMPONENTS.RAMB18E2"}));
}

BOOST_AUTO_TEST_SUITE_END()
