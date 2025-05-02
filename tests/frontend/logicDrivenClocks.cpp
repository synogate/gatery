/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2025 Michael Offel, Andreas Ley

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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


BOOST_AUTO_TEST_SUITE(logicDrivenClk);

BOOST_FIXTURE_TEST_CASE(not_running_clock, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	Bit clockSignal = pinIn().setName("clockSignal");
	clock.overrideClkWith(clockSignal);

	ClockScope clockScope(clock);

	UInt counter(4_b);
	counter = reg(counter+1, 0);

	pinOut(counter, "counter");


	addSimulationProcess([=, this, &clock]()->SimProcess {
		simu(clockSignal) = '0';

		BOOST_TEST(simu(counter) == 0);

		for (auto i : Range(500)) {
			co_await WaitFor(Seconds{1, 5} / clock.getClk()->absoluteFrequency());

			BOOST_TEST(simu(counter) == 0);
		}

		stopTest();
	});


	runTest(1000u / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(counter, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	UInt counter(4_b);

	Bit clockEnable = pinIn().setName("clockEnable");
	IF (clockEnable)
		counter += 1;

	counter = reg(counter, 0);

	Clock logicDrivenClock({ .absoluteFrequency = 100'000 });
	logicDrivenClock.overrideClkWith(counter.msb());

	UInt counter2(4_b);
	{
		ClockScope clockScope(logicDrivenClock);
		counter2 = reg(counter2+1, 0);
	}

	pinOut(counter, "counter");
	pinOut(counter2, "counter2");


	addSimulationProcess([=, this, &clock]()->SimProcess {
		simu(clockEnable) = '0';

		for (auto i : Range(64)) {
			co_await AfterClk(clock);
			BOOST_TEST(simu(counter2) == 0);
		}

		simu(clockEnable) = '1';

		size_t expectedValue = 0;
		for (auto i : Range(64)) {
			co_await AfterClk(clock);
			if (i % 16 == 8)
				expectedValue++;
			BOOST_TEST(simu(counter2) == expectedValue);
		}

		stopTest();
	});


	runTest(1000u / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(waitOnClk, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	UInt counter(4_b);

	Bit clockEnable = pinIn().setName("clockEnable");
	IF (clockEnable)
		counter += 1;

	counter = reg(counter, 0);

	Clock logicDrivenClock({ .absoluteFrequency = 100'000 });
	logicDrivenClock.overrideClkWith(counter.msb());

	UInt counter2(4_b);
	{
		ClockScope clockScope(logicDrivenClock);
		counter2 = reg(counter2+1, 0);
	}

	pinOut(counter, "counter");
	pinOut(counter2, "counter2");




	addSimulationProcess([=, this, &clock]()->SimProcess {

		size_t clockCyclesExpected = 0;

		fork([&]()->SimProcess {
			size_t clockCyclesSeen = 0;
			size_t expectedCounter2Value = 0;
			while (true) {
				co_await OnClk(logicDrivenClock);
				BOOST_ASSERT(simu(counter2) == expectedCounter2Value);// Should see the old value from before the clock triggered because of OnClk.
				expectedCounter2Value++;

				BOOST_ASSERT(simu(counter) == 7); // Should see the old value from before the clock triggered because of OnClk.

				clockCyclesSeen++;
				BOOST_ASSERT(clockCyclesSeen == clockCyclesExpected);
			}
		});

		simu(clockEnable) = '0';

		for (auto i : Range(64))
			co_await AfterClk(clock);

		simu(clockEnable) = '1';

		for (auto i : Range(64)) {
			clockCyclesExpected++;
			co_await AfterClk(clock);
		}

		stopTest();
	});


	runTest(1000u / clock.getClk()->absoluteFrequency());
}


BOOST_AUTO_TEST_SUITE_END();