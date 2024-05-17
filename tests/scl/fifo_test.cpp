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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/Simulator.h>
#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>


#include <gatery/scl/TransactionalFifo.h>
#include <gatery/scl/FifoArray.h>

#include <gatery/scl/synthesisTools/IntelQuartus.h>
#include <gatery/scl/arch/intel/IntelDevice.h>

#include <queue>

using namespace boost::unit_test;
using namespace gtry;

struct FifoTest
{
	FifoTest(Clock& clk) : rdClk(clk), wrClk(clk) {}
	FifoTest(Clock& rdClk, Clock& wrClk) : rdClk(rdClk), wrClk(wrClk) {}

	template<typename T = scl::Fifo<UInt>>
	T create(size_t depth, BitWidth width, bool generate = true, scl::FifoLatency latency = scl::FifoLatency(1))
	{
		T fifo{ depth, UInt{ width }, latency};
		actualDepth = fifo.depth();

		{
			ClockScope clkScp(wrClk);
	 		pushData = width;

			IF(push)
				fifo.push(pushData);

			push = pinIn().setName("push_valid");
			pushData = pinIn(width).setName("push_data");

			full = fifo.full();
			pinOut(full).setName("full");
		}
		{
			ClockScope clkScp(rdClk);
			popData = fifo.peek();
			IF(pop)
				fifo.pop();

			pop = pinIn().setName("pop_ready");
			pinOut(popData).setName("pop_data");

			empty = fifo.empty();
			pinOut(empty).setName("empty");
		}
		
		if(generate)
			fifo.generate();
		return fifo;
	}

	SimProcess operator() ()
	{
		while (!model.empty())
			model.pop();
		while (true)
		{
			co_await OnClk(wrClk);

			if (simu(full) == '1')
				BOOST_TEST(!model.empty());

			if (simu(push) && !simu(full))
				model.push(uint8_t(simu(pushData)));

			if (!simu(empty))
			{
				uint8_t peekValue = (uint8_t)simu(popData);
				BOOST_TEST(!model.empty());
				if(!model.empty())
					BOOST_TEST(peekValue == model.front());
			}

			if (simu(pop) && !simu(empty))
			{
				if(!model.empty())
					model.pop();
			}
		}
	}

	std::function<SimProcess()> writeProcess()
	{
		return [this]()->SimProcess{
			while (!model.empty())
				model.pop();

			while (true)
			{
				co_await OnClk(wrClk);

				if (simu(full) == '1')
					BOOST_TEST(!model.empty());

				if (simu(push) && !simu(full))
					model.push(uint8_t(simu(pushData)));
			}
		};
	}

	std::function<SimProcess()> readProcess()
	{
		return [this]()->SimProcess{

			auto *sim = sim::SimulationContext::current()->getSimulator();

			while (!model.empty())
				model.pop();

			while (true)
			{
				co_await OnClk(rdClk);

				if (!simu(empty))
				{
					uint8_t peekValue = (uint8_t)simu(popData);
					BOOST_TEST(!model.empty());
					if(!model.empty())
						BOOST_TEST(peekValue == model.front(), (size_t)peekValue << " == " << (size_t)model.front() << " does not hold at simulation time " << sim->getCurrentSimulationTime().numerator()/(double)sim->getCurrentSimulationTime().denominator() * 1e9 << "ns");
				}

				if (simu(pop) && !simu(empty))
				{
					if(!model.empty())
						model.pop();
				}
			}
		};
	}

	Clock rdClk;
	Clock wrClk;

	UInt pushData;
	Bit push;

	UInt popData;
	Bit pop;

	Bit empty;
	Bit full;

	size_t actualDepth;

	std::queue<uint8_t> model;
};

BOOST_FIXTURE_TEST_CASE(Fifo_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);
 
	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b);

	size_t actualDepth = fifo.actualDepth;

	OutputPin halfEmpty = pinOut(uut.almostEmpty(actualDepth/2)).setName("half_empty");
	OutputPin halfFull = pinOut(uut.almostFull(actualDepth/2)).setName("half_full");
	
	addSimulationProcess([=,this]()->SimProcess {
		simu(fifo.pushData) = 0;
		simu(fifo.push) = '0';
		simu(fifo.pop) = '0';

		for(size_t i = 0; i < 5; ++i)
			co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(fifo.push) = '1';
			simu(fifo.pushData) = i * 3;
			co_await AfterClk(clock);
		}
		simu(fifo.push) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '0');
		BOOST_TEST(simu(fifo.full) == '1');
		BOOST_TEST(simu(halfEmpty) == '0');
		BOOST_TEST(simu(halfFull) == '1');

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(fifo.pop) = '1';
			co_await AfterClk(clock);
		}

		simu(fifo.pop) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		for (size_t i = 0, count = 0; count < actualDepth/2; ++i)
		{
			bool doPush = i % 15 != 0;
			bool doPop = count > 0 && (i % 8 != 0);
			simu(fifo.push) = doPush;
			simu(fifo.pushData) = uint8_t(i * 5);
			simu(fifo.pop) = doPop;
			co_await AfterClk(clock);

			if (doPush) count++;
			if (doPop) count--;
		}

		simu(fifo.push) = '0';
		simu(fifo.pop) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '0');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '1');

		for (size_t i = 0; i < actualDepth/2; ++i)
		{
			simu(fifo.pop) = '1';
			co_await AfterClk(clock);
		}

		simu(fifo.pop) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		stopTest();
	});

	addSimulationProcess(fifo);

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("after");

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(Fifo_fuzz, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	fifo.create(16, 8_b);

	addSimulationProcess([&]()->SimProcess {
		simu(fifo.pushData) = 0;

		std::mt19937 rng{ 12524 };
		while (true)
		{
			if (!simu(fifo.full) && (rng() % 2 == 0))
			{
				simu(fifo.push) = '1';
				simu(fifo.pushData) = uint8_t(rng());
			}
			else
			{
				simu(fifo.push) = '0';
			}

			if (!simu(fifo.empty) && (rng() % 2 == 0))
			{
				simu(fifo.pop) = '1';
			}
			else
			{
				simu(fifo.pop) = '0';
			}

			co_await AfterClk(clock);
		}
	});

	addSimulationProcess(fifo);

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo_fuzz.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("fifo_fuzz");

	runTicks(clock.getClk(), 2048);
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create<scl::TransactionalFifo<UInt>>(16, 8_b, false);

	size_t actualDepth = fifo.actualDepth;

	OutputPin halfEmpty = pinOut(uut.almostEmpty(actualDepth / 2)).setName("half_empty");
	OutputPin halfFull = pinOut(uut.almostFull(actualDepth / 2)).setName("half_full");
	
	InputPin pushCommit = pinIn().setName("pushCommit");
	IF(pushCommit)
		uut.commitPush();
	InputPin pushRollback = pinIn().setName("pushRollback");
	IF(pushRollback)
		uut.rollbackPush();
	InputPin popCommit = pinIn().setName("popCommit");
	IF(popCommit)
		uut.commitPop();
	InputPin popRollback = pinIn().setName("popRollback");
	IF(popRollback)
		uut.rollbackPop();

	uut.generate();

	addSimulationProcess([=, this]()->SimProcess {
		simu(fifo.pushData) = 0;
		simu(fifo.push) = '0';
		simu(fifo.pop) = '0';

		simu(pushCommit) = '0';
		simu(pushRollback) = '0';
		simu(popCommit) = '0';
		simu(popRollback) = '0';

		for(size_t i = 0; i < 5; ++i)
			co_await AfterClk(clock);

		for(size_t c : {0, 1})
		{
			BOOST_TEST(simu(fifo.empty) == '1');
			BOOST_TEST(simu(fifo.full) == '0');
			BOOST_TEST(simu(halfEmpty) == '1');
			BOOST_TEST(simu(halfFull) == '0');

			for(size_t i = 0; i < actualDepth; ++i)
			{
				simu(fifo.push) = '1';
				simu(fifo.pushData) = i * 3;
				co_await AfterClk(clock);
			}
			simu(fifo.push) = '0';
			co_await AfterClk(clock);

			BOOST_TEST(simu(fifo.empty) == '1');
			BOOST_TEST(simu(fifo.full) == '1');
			BOOST_TEST(simu(halfEmpty) == '1');
			BOOST_TEST(simu(halfFull) == '1');

			if(c == 0)
			{
				simu(pushRollback) = '1';
				co_await AfterClk(clock);
				simu(pushRollback) = '0';
			}
		}

		simu(pushCommit) = '1';
		co_await AfterClk(clock);
		simu(pushCommit) = '0';

		for(size_t c : {0, 1})
		{
			BOOST_TEST(simu(fifo.empty) == '0');
			BOOST_TEST(simu(fifo.full) == '1');
			BOOST_TEST(simu(halfEmpty) == '0');
			BOOST_TEST(simu(halfFull) == '1');

			for(size_t i = 0; i < actualDepth; ++i)
			{
				simu(fifo.pop) = '1';
				co_await AfterClk(clock);
			}

			simu(fifo.pop) = '0';
			co_await AfterClk(clock);

			BOOST_TEST(simu(fifo.empty) == '1');
			BOOST_TEST(simu(fifo.full) == '1');
			BOOST_TEST(simu(halfEmpty) == '1');
			BOOST_TEST(simu(halfFull) == '1');

			if(c == 0)
			{
				simu(popRollback) = '1';
				co_await AfterClk(clock);
				simu(popRollback) = '0';
			}
		}
		simu(popCommit) = '1';
		co_await AfterClk(clock);
		simu(popCommit) = '0';

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		stopTest();
	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("after");

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_cutoff, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create<scl::TransactionalFifo<UInt>>(16, 8_b, false);

	size_t actualDepth = fifo.actualDepth;

	OutputPin halfEmpty = pinOut(uut.almostEmpty(actualDepth / 2)).setName("half_empty");
	OutputPin halfFull = pinOut(uut.almostFull(actualDepth / 2)).setName("half_full");

	InputPins pushCuttoff = pinIn(5_b).setName("pushCutoff");
	InputPin pushCommit = pinIn().setName("pushCommit");
	IF(pushCommit)
		uut.commitPush(pushCuttoff);
	InputPin pushRollback = pinIn().setName("pushRollback");
	IF(pushRollback)
		uut.rollbackPush();
	InputPin popCommit = pinIn().setName("popCommit");
	IF(popCommit)
		uut.commitPop();
	InputPin popRollback = pinIn().setName("popRollback");
	IF(popRollback)
		uut.rollbackPop();

	uut.generate();

	addSimulationProcess([=, this]()->SimProcess {
		simu(fifo.pushData) = 0;
		simu(fifo.push) = '0';
		simu(fifo.pop) = '0';

		simu(pushCuttoff) = 2;
		simu(pushCommit) = '0';
		simu(pushRollback) = '0';
		simu(popCommit) = '0';
		simu(popRollback) = '0';

		for(size_t i = 0; i < 5; ++i)
			co_await AfterClk(clock);

		for(size_t i = 0; i < actualDepth; ++i)
		{
			simu(fifo.push) = '1';
			simu(fifo.pushData) = i * 3;
			co_await AfterClk(clock);
		}
		simu(fifo.push) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '1');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '1');

		simu(pushCommit) = '1';
		co_await AfterClk(clock);
		simu(pushCommit) = '0';

		BOOST_TEST(simu(fifo.empty) == '0');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '0');
		BOOST_TEST(simu(halfFull) == '1');

		for(size_t i = 0; i < actualDepth - 2; ++i)
		{
			simu(fifo.pop) = '1';
			BOOST_TEST(simu(fifo.empty) == '0');
			co_await AfterClk(clock);
		}

		simu(fifo.pop) = '0';
		co_await AfterClk(clock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '1');

		stopTest();
	});

	addSimulationProcess(fifo);

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "fifo.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("after");

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(DualClockFifo, BoostUnitTestSimulationFixture)
{
	Clock rdClock({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(rdClock);
	Clock wrClock({ .absoluteFrequency = 133'000'000 });
	HCL_NAMED(wrClock);
 
	FifoTest fifo{ rdClock, wrClock };
	auto&& uut = fifo.create(16, 8_b, true, scl::FifoLatency::DontCare());

	size_t actualDepth = fifo.actualDepth;

	Bit halfEmpty;
	Bit halfFull;
	{
		ClockScope scope(rdClock);
		halfEmpty = uut.almostEmpty(actualDepth/2);
		pinOut(halfEmpty).setName("half_empty");
	}
	{
		ClockScope scope(wrClock);
		halfFull = uut.almostFull(actualDepth/2);
		pinOut(halfFull).setName("half_full");
	}
	
	addSimulationProcess([=,this]()->SimProcess {
		simu(fifo.pushData) = 0;
		simu(fifo.push) = '0';
		simu(fifo.pop) = '0';

		for(size_t i = 0; i < 5; ++i)
			co_await AfterClk(wrClock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(fifo.push) = '1';
			simu(fifo.pushData) = i * 3;
			co_await AfterClk(wrClock);
		}
		simu(fifo.push) = '0';
		co_await AfterClk(wrClock);

		BOOST_TEST(simu(fifo.full) == '1');
		BOOST_TEST(simu(halfFull) == '1');

		co_await AfterClk(wrClock);
		co_await AfterClk(wrClock);
		co_await AfterClk(wrClock);

		BOOST_TEST(simu(fifo.empty) == '0');
		BOOST_TEST(simu(halfEmpty) == '0');

		co_await AfterClk(rdClock);

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(fifo.pop) = '1';
			co_await AfterClk(rdClock);
		}

		simu(fifo.pop) = '0';
		co_await AfterClk(rdClock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(halfEmpty) == '1');

		co_await AfterClk(rdClock);
		co_await AfterClk(rdClock);
		co_await AfterClk(rdClock);

		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfFull) == '0');

		stopTest();
	});

	addSimulationProcess(fifo.readProcess());
	addSimulationProcess(fifo.writeProcess());

	design.postprocess();

	#if 0
		vhdl::VHDLExport vhdl("vhdl_DCFIFO_quartus/");
		vhdl.addTestbenchRecorder(getSimulator(), "testbench", false);
		vhdl.targetSynthesisTool(new IntelQuartus());
		vhdl.writeProjectFile("import_IPCore.tcl");
		vhdl.writeStandAloneProjectFile("IPCore.qsf");
		vhdl.writeConstraintsFile("constraints.sdc");
		vhdl.writeClocksFile("clocks.sdc");
		vhdl(design.getCircuit());	
	#endif

	runTest(hlim::ClockRational(20000, 1) / rdClock.getClk()->absoluteFrequency());

}


BOOST_FIXTURE_TEST_CASE(FifoArray_poc, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope scope(clk);

	if (true) {
		//const std::string boardName = "cyc1000";
		const std::string fpgaNumber = "AGFB014R24B2E2V";
		auto device = std::make_unique<scl::IntelDevice>();
		device->setupDevice(fpgaNumber);
		design.setTargetTechnology(std::move(device));
	}

	size_t numberOfFifos = 4;
	size_t elementsPerFifo = 64;
	BitWidth dataW = 4_b;


	scl::FifoArray<UInt> dutFifo(numberOfFifos, elementsPerFifo, UInt{dataW});

	Bit pushEnable; pinIn(pushEnable, "pushEnable");
	UInt pushSelector = BitWidth::count(numberOfFifos); pinIn(pushSelector, "pushSelector");
	UInt pushData = dontCare(UInt{ dataW }); pinIn(pushData, "pushData");

	dutFifo.selectPush(pushSelector);
	pinOut(dutFifo.full(), "pushFull");
	IF(pushEnable) dutFifo.push(pushData);

	Bit popEnable; pinIn(popEnable, "popEnable");
	UInt popSelector = constructFrom(pushSelector); pinIn(popSelector, "popSelector");
	UInt popData = reg(dutFifo.peek(), RegisterSettings{ .allowRetimingBackward = true });
	pinOut(popData, "popData");

	
	dutFifo.selectPop(popSelector);
	pinOut(dutFifo.empty(), "popEmpty");
	IF(popEnable) dutFifo.pop();


	dutFifo.generate();



	addSimulationProcess([&, this]()->SimProcess {
		simu(pushEnable) = '0';
		simu(pushSelector) = 0;
		simu(pushData) = 13;
		simu(popSelector) = 0;
		simu(popEnable) = '0';

		co_await WaitFor(Seconds{0});

		BOOST_TEST(simu(dutFifo.full()) == '0');
		BOOST_TEST(simu(dutFifo.empty()) == '1');

		simu(pushEnable) = '1';
		
		co_await OnClk(clk);

		simu(pushEnable) = '0';

		co_await WaitFor(Seconds{0});

		BOOST_TEST(simu(dutFifo.empty()) == '0');
		simu(popEnable) = '1';

		co_await OnClk(clk);
		simu(popEnable) = '0';
		co_await OnClk(clk);
		BOOST_TEST(simu(popData) == 13);
		co_await OnClk(clk);
		co_await OnClk(clk);


		stopTest();
	});



	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}


/*

BOOST_FIXTURE_TEST_CASE(DC_TransactionalFifo_basic, BoostUnitTestSimulationFixture)
{
	Clock rdClock({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(rdClock);
	Clock wrClock({ .absoluteFrequency = 133'000'000 });
	HCL_NAMED(wrClock);
 
	FifoTest fifo{ rdClock, wrClock };
	auto&& uut = fifo.create<scl::TransactionalFifo<UInt>>(16, 8_b, false);

	size_t actualDepth = fifo.actualDepth;



	Bit pushCommit;
	Bit pushRollback;
	Bit popCommit;
	Bit popRollback;

	Bit halfEmpty;
	Bit halfFull;
	{
		ClockScope scope(rdClock);
		halfEmpty = uut.almostEmpty(actualDepth/2);
		pinOut(halfEmpty).setName("half_empty");

		popCommit = pinIn().setName("popCommit");
		IF(popCommit)
			uut.commitPop();
		popRollback = pinIn().setName("popRollback");
		IF(popRollback)
			uut.rollbackPop();

	}
	{
		ClockScope scope(wrClock);
		halfFull = uut.almostFull(actualDepth/2);
		pinOut(halfFull).setName("half_full");

		pushCommit = pinIn().setName("pushCommit");
		IF(pushCommit)
			uut.commitPush();
		pushRollback = pinIn().setName("pushRollback");
		IF(pushRollback)
			uut.rollbackPush();
	}
	
	uut.generate();

	addSimulationProcess([=, this]()->SimProcess {
		simu(fifo.pushData) = 0;
		simu(fifo.push) = 0;
		simu(fifo.pop) = 0;

		simu(pushCommit) = 0;
		simu(pushRollback) = 0;
		simu(popCommit) = 0;
		simu(popRollback) = 0;

		for(size_t i = 0; i < 5; ++i)
			co_await AfterClk(wrClock);

		for(size_t c : {0, 1})
		{
			BOOST_TEST(simu(fifo.empty) == 1);
			BOOST_TEST(simu(fifo.full) == 0);
			BOOST_TEST(simu(halfEmpty) == 1);
			BOOST_TEST(simu(halfFull) == 0);

			for(size_t i = 0; i < actualDepth; ++i)
			{
				simu(fifo.push) = '1';
				simu(fifo.pushData) = i * 3;
				co_await AfterClk(wrClock);
			}
			simu(fifo.push) = '0';
			co_await AfterClk(wrClock);

			BOOST_TEST(simu(fifo.full) == 1);
			BOOST_TEST(simu(halfFull) == 1);

			co_await AfterClk(wrClock);
			co_await AfterClk(wrClock);
			co_await AfterClk(wrClock);

			BOOST_TEST(simu(fifo.empty) == 1);
			BOOST_TEST(simu(halfEmpty) == 1);

			if(c == 0)
			{
				simu(pushRollback) = '1';
				co_await AfterClk(wrClock);
				simu(pushRollback) = '0';
			}
		}

		simu(pushCommit) = '1';
		co_await AfterClk(wrClock);
		simu(pushCommit) = '0';

		co_await AfterClk(wrClock);
		co_await AfterClk(wrClock);
		co_await AfterClk(wrClock);
		co_await AfterClk(rdClock);

		for(size_t c : {0, 1})
		{
			BOOST_TEST(simu(fifo.empty) == 0);
			BOOST_TEST(simu(halfEmpty) == 0);

			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);

			BOOST_TEST(simu(fifo.full) == 1);
			BOOST_TEST(simu(halfFull) == 1);

			for(size_t i = 0; i < actualDepth; ++i)
			{
				simu(fifo.pop) = '1';
				co_await AfterClk(rdClock);
			}

			simu(fifo.pop) = '0';
			co_await AfterClk(rdClock);

			BOOST_TEST(simu(fifo.empty) == 1);
			BOOST_TEST(simu(halfEmpty) == 1);

			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);
			co_await AfterClk(rdClock);

			BOOST_TEST(simu(fifo.full) == 1);
			BOOST_TEST(simu(halfFull) == 1);

			if(c == 0)
			{
				simu(popRollback) = '1';
				co_await AfterClk(rdClock);
				simu(popRollback) = '0';
			}
		}
		simu(popCommit) = '1';
		co_await AfterClk(rdClock);
		simu(popCommit) = '0';

		BOOST_TEST(simu(fifo.empty) == 1);
		BOOST_TEST(simu(halfEmpty) == 1);

		co_await AfterClk(rdClock);
		co_await AfterClk(rdClock);
		co_await AfterClk(rdClock);
		co_await AfterClk(rdClock);

		BOOST_TEST(simu(fifo.full) == 0);
		BOOST_TEST(simu(halfFull) == 0);

		stopTest();
	});

	design.postprocess();

	runTest(hlim::ClockRational(20000, 1) / rdClock.getClk()->absoluteFrequency());
}

*/

