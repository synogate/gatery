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
			*push = width;
			scl::strm::push(fifo, move(push));
			pinIn(push, "push");

			full = fifo.full();
			pinOut(full, "full");
		}
		{
			ClockScope clkScp(rdClk);
			pop = scl::strm::pop(fifo);
			pinOut(pop, "pop");

			empty = fifo.empty();
			pinOut(empty, "empty");
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

			if (simu(valid(push)) && simu(ready(push)))
				model.push(uint8_t(simu(*push)));

			if (simu(valid(pop)))
			{
				uint8_t peekValue = (uint8_t)simu(*pop);
				BOOST_TEST(!model.empty());
				if(!model.empty())
					BOOST_TEST(peekValue == model.front());
			}

			if (simu(valid(pop)) && simu(ready(pop)))
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
				co_await performTransferWait(push, wrClk);
				model.push(uint8_t(simu(*push)));
			}
		};
	}

	std::function<SimProcess()> readProcess()
	{
		return [this]()->SimProcess {
			while (!model.empty())
				model.pop();

			while (true)
			{
				co_await OnClk(rdClk);

				if (simu(valid(pop)))
				{
					uint8_t peekValue = (uint8_t)simu(*pop);
					BOOST_TEST(!model.empty());
					if(!model.empty())
					{
						sim::Simulator* sim = sim::SimulationContext::current()->getSimulator();
						BOOST_TEST(peekValue == model.front(), 
							(size_t)peekValue << " == " << (size_t)model.front() << 
							" does not hold at simulation time " << sim->getCurrentSimulationTime().numerator() / (double)sim->getCurrentSimulationTime().denominator() * 1e9 << "ns"
						);
					}
				}

				if (simu(valid(pop)) && simu(ready(pop)))
				{
					BOOST_TEST(!model.empty());
					if(!model.empty())
						model.pop();
				}
			}
		};
	}

	SimProcess basicTest(const Clock& clock)
	{
		simu(valid(push)) = '0';
		simu(ready(pop)) = '0';
		co_await OnClk(clock);

		BOOST_TEST(simu(empty) == '1');
		BOOST_TEST(simu(full) == '0');

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(*push) = i * 3;
			co_await performTransfer(push, clock);
		}
		co_await OnClk(clock);

		BOOST_TEST(simu(empty) == '0');
		BOOST_TEST(simu(full) == '1');

		// test simultaneous push and pop on full fifo
		fork(sendPacket(push, scl::strm::SimPacket(0x04030201, 32_b), clock));

		simu(ready(pop)) = '1';
		for (size_t i = 0; i < 4; ++i)
			co_await performTransferWait(pop, clock);
		simu(ready(pop)) = '0';

		// clear fifo
		simu(ready(pop)) = '1';
		for (size_t i = 0; i < actualDepth; ++i)
			co_await performTransferWait(pop, clock);
		simu(ready(pop)) = '0';
		co_await OnClk(clock);

		BOOST_TEST(simu(empty) == '1');
		BOOST_TEST(simu(full) == '0');

		for (size_t i = 0, count = 0; count < actualDepth / 2; ++i)
		{
			bool doPush = i % 15 != 0;
			bool doPop = simu(valid(pop)) && (i % 8 != 0);

			simu(valid(push)) = doPush;
			simu(*push) = uint8_t(i * 5);
			simu(ready(pop)) = doPop;
			co_await OnClk(clock);

			if (doPush) count++;
			if (doPop) count--;
		}

		simu(valid(push)) = '0';
		simu(ready(pop)) = '0';
		co_await OnClk(clock);

		BOOST_TEST(simu(empty) == '0');
		BOOST_TEST(simu(full) == '0');

		simu(ready(pop)) = '1';
		for (size_t i = 0; i < actualDepth / 2; ++i)
			co_await OnClk(clock);
		simu(ready(pop)) = '0';

		co_await OnClk(clock);

		BOOST_TEST(simu(empty) == '1');
		BOOST_TEST(simu(full) == '0');

		// test push and pop from empty fifo
		fork(sendPacket(push, scl::strm::SimPacket(0x14131211, 32_b), clock));

		simu(ready(pop)) = '1';
		for (size_t i = 0; i < 4; ++i)
			co_await performTransferWait(pop, clock);
		simu(ready(pop)) = '0';

		co_await OnClk(clock);
		BOOST_TEST(simu(empty) == '1');
		BOOST_TEST(simu(full) == '0');
	}

	Clock rdClk;
	Clock wrClk;

	scl::RvStream<UInt> push;
	scl::RvStream<UInt> pop;

	Bit empty;
	Bit full;

	size_t actualDepth;

	std::queue<uint8_t> model;
};

BOOST_FIXTURE_TEST_CASE(Fifo_basic_latency_0, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b, true, 0);

	addSimulationProcess([&, this]()->SimProcess {
		co_await fifo.basicTest(clock);
		stopTest();
		});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(Fifo_basic_latency_1, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);
 
	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b, true, 1);

	addSimulationProcess([&, this]()->SimProcess {
		co_await fifo.basicTest(clock);
		stopTest();
	});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(Fifo_basic_latency_2, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b, true, 2);

	addSimulationProcess([&, this]()->SimProcess {
		co_await fifo.basicTest(clock);
		stopTest();
	});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(Fifo_basic_latency_3, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b, true, 3);

	addSimulationProcess([&, this]()->SimProcess {
		co_await fifo.basicTest(clock);
		stopTest();
	});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(Fifo_almost, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create(16, 8_b, true, 1);

	OutputPin halfEmpty = pinOut(uut.almostEmpty(fifo.actualDepth /2)).setName("half_empty");
	OutputPin halfFull = pinOut(uut.almostFull(fifo.actualDepth /2)).setName("half_full");

	addSimulationProcess([&, this]()->SimProcess {
		simu(valid(fifo.push)) = '0';
		simu(ready(fifo.pop)) = '0';
		co_await OnClk(clock);

		simu(valid(fifo.push)) = '1';
		simu(*fifo.push) = 0;
		for (size_t i = 0; i < fifo.actualDepth / 2; ++i)
		{
			BOOST_TEST(simu(halfFull) == '0');
			BOOST_TEST(simu(halfEmpty) == '1');
			co_await OnClk(clock);
		}
		BOOST_TEST(simu(halfFull) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');

		co_await OnClk(clock);
		BOOST_TEST(simu(halfFull) == '1');
		BOOST_TEST(simu(halfEmpty) == '1');
		simu(valid(fifo.push)) = '0';

		co_await OnClk(clock);
		BOOST_TEST(simu(halfFull) == '1');
		BOOST_TEST(simu(halfEmpty) == '0');

		stopTest();
	});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_basic_1, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create<scl::TransactionalFifo<UInt>>(16, 8_b, true, 1);

	OutputPin halfEmpty = pinOut(uut.almostEmpty(fifo.actualDepth / 2)).setName("half_empty");
	OutputPin halfFull = pinOut(uut.almostFull(fifo.actualDepth / 2)).setName("half_full");

	addSimulationProcess([&, this]()->SimProcess {
		co_await fifo.basicTest(clock);

		simu(valid(fifo.push)) = '1';
		simu(*fifo.push) = 0;
		for (size_t i = 0; i < fifo.actualDepth / 2; ++i)
		{
			BOOST_TEST(simu(halfFull) == '0');
			BOOST_TEST(simu(halfEmpty) == '1');
			co_await OnClk(clock);
		}
		BOOST_TEST(simu(halfFull) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');

		co_await OnClk(clock);
		BOOST_TEST(simu(halfFull) == '1');
		BOOST_TEST(simu(halfEmpty) == '1');
		simu(valid(fifo.push)) = '0';

		co_await OnClk(clock);
		BOOST_TEST(simu(halfFull) == '1');
		BOOST_TEST(simu(halfEmpty) == '0');

		stopTest();
		});

	addSimulationProcess([&]() { return fifo(); });

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_commit_rollback, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
	ClockScope clkScp(clock);

	FifoTest fifo{ clock };
	auto&& uut = fifo.create<scl::TransactionalFifo<UInt>>(16, 8_b, false);

	size_t actualDepth = fifo.actualDepth;
	
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

	addSimulationProcess([&, this]()->SimProcess {
		simu(valid(fifo.push)) = '0';
		simu(ready(fifo.pop)) = '0';
		simu(pushCommit) = '0';
		simu(pushRollback) = '0';
		simu(popCommit) = '0';
		simu(popRollback) = '0';
		co_await OnClk(clock);

		// push rollback
		co_await sendPacket(fifo.push, scl::strm::SimPacket(0x14131211, 32_b), clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '0');

		simu(pushRollback) = '1';
		co_await OnClk(clock);
		simu(pushRollback) = '0';
		BOOST_TEST(simu(valid(fifo.pop)) == '0');

		// push commit
		co_await sendPacket(fifo.push, scl::strm::SimPacket(0xAABBCCDD, 32_b), clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '0');

		simu(pushCommit) = '1';
		co_await OnClk(clock);
		simu(pushCommit) = '0';

		while(simu(valid(fifo.pop)) == '0')
			co_await OnClk(clock);
		BOOST_TEST(simu(*fifo.pop) == 0xDD);

		// pop rollback
		simu(ready(fifo.pop)) = '1';
		while (simu(valid(fifo.pop)) == '1')
			co_await OnClk(clock);

		simu(popRollback) = '1';
		co_await OnClk(clock);
		simu(popRollback) = '0';

		while (simu(valid(fifo.pop)) == '0')
			co_await OnClk(clock);
		BOOST_TEST(simu(*fifo.pop) == 0xDD);

		// pop commit
		simu(valid(fifo.push)) = '1';
		simu(*fifo.push) = 1;
		while(simu(ready(fifo.push)) == '1')
			co_await OnClk(clock);

		simu(ready(fifo.pop)) = '1';
		while (simu(valid(fifo.pop)) == '1')
			co_await OnClk(clock);

		simu(popCommit) = '1';
		co_await OnClk(clock);
		simu(popCommit) = '0';

		while (simu(ready(fifo.push)) == '0')
			co_await OnClk(clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}


BOOST_FIXTURE_TEST_CASE(TransactionalFifo_cutoff, BoostUnitTestSimulationFixture)
{
	Clock clock({ 100_mhz });
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

	addSimulationProcess([&, this]()->SimProcess {
		simu(valid(fifo.push)) = '0';
		simu(ready(fifo.pop)) = '0';
		simu(pushCuttoff) = 2;
		simu(pushCommit) = '0';
		simu(pushRollback) = '0';
		simu(popCommit) = '0';
		simu(popRollback) = '0';
		co_await OnClk(clock);

		// push commit
		co_await sendPacket(fifo.push, scl::strm::SimPacket(0xAABBCCDD, 32_b), clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '0');

		simu(pushCommit) = '1';
		co_await OnClk(clock);
		simu(pushCommit) = '0';

		while (simu(valid(fifo.pop)) == '0')
			co_await OnClk(clock);
		BOOST_TEST(simu(*fifo.pop) == 0xDD);

		simu(ready(fifo.pop)) = '1';
		co_await OnClk(clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '1');
		BOOST_TEST(simu(*fifo.pop) == 0xDD);

		co_await OnClk(clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '1');
		BOOST_TEST(simu(*fifo.pop) == 0xCC);

		co_await OnClk(clock);
		BOOST_TEST(simu(valid(fifo.pop)) == '0');

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(10us));
}

BOOST_FIXTURE_TEST_CASE(DualClockFifo, BoostUnitTestSimulationFixture)
{
	Clock rdClock({ .absoluteFrequency = 100_mhz, .name = "rdClock" });
	Clock wrClock({ .absoluteFrequency = 133_mhz, .name = "wrClock" });
 
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
	
	addSimulationProcess([&, this]()->SimProcess {
		simu(valid(fifo.push)) = '0';
		simu(ready(fifo.pop)) = '0';
		co_await AfterClk(wrClock);

		BOOST_TEST(simu(fifo.empty) == '1');
		BOOST_TEST(simu(fifo.full) == '0');
		BOOST_TEST(simu(halfEmpty) == '1');
		BOOST_TEST(simu(halfFull) == '0');

		for (size_t i = 0; i < actualDepth; ++i)
		{
			simu(valid(fifo.push)) = '1';
			simu(*fifo.push) = i * 3;
			co_await AfterClk(wrClock);
		}
		simu(valid(fifo.push)) = '0';
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
			simu(ready(fifo.pop)) = '1';
			co_await AfterClk(rdClock);
		}

		simu(ready(fifo.pop)) = '0';
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
	BOOST_TEST(!runHitsTimeout(10us));
}
