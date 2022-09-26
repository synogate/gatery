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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <random>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(SimProc_Basics, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	{
		ClockScope clkScp(clock);

		UInt counter(8_b);
		counter = reg(counter, 0);

		auto incrementPin = pinIn(8_b);
		auto outputPin = pinOut(counter);
		counter += incrementPin;

		addSimulationProcess([=]()->SimProcess{
			co_await WaitFor(Seconds(1, 2)/clock.absoluteFrequency());
			for (auto i : Range(10)) {
				simu(incrementPin) = i;
				co_await WaitFor(Seconds(5)/clock.absoluteFrequency());
			}
		});
		addSimulationProcess([=]()->SimProcess{

			co_await WaitClk(clock);

			size_t expectedSum = 0;

			while (true) {
				expectedSum += simu(incrementPin);

				BOOST_TEST(expectedSum == simu(outputPin));
				BOOST_TEST(simu(outputPin).defined() == 0xFF);

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}



BOOST_FIXTURE_TEST_CASE(SimProc_BigInt_small, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	{
		ClockScope clkScp(clock);

		UInt counter(40_b);
		counter = reg(counter, 0);

		auto incrementPin = pinIn(40_b);
		auto outputPin = pinOut(counter);
		counter += incrementPin;

		addSimulationProcess([=]()->SimProcess{
			co_await WaitFor(Seconds(1, 2)/clock.absoluteFrequency());
			for (auto i : Range(10)) {
				BigInt v = i;
				v |= BigInt(i*13) << 20;
				simu(incrementPin) = v;
				co_await WaitFor(Seconds(5)/clock.absoluteFrequency());
			}
		});
		addSimulationProcess([=]()->SimProcess{

			co_await WaitClk(clock);

			BigInt expectedSum = 0;

			while (true) {
				expectedSum += (BigInt) simu(incrementPin);

				BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
				BOOST_TEST(simu(outputPin).allDefined());

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}



BOOST_FIXTURE_TEST_CASE(SimProc_BigInt, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	{
		ClockScope clkScp(clock);

		UInt counter(128_b);
		counter = reg(counter, 0);

		auto incrementPin = pinIn(128_b);
		auto outputPin = pinOut(counter);
		counter ^= incrementPin;

		addSimulationProcess([=]()->SimProcess{
			co_await WaitFor(Seconds(1, 2)/clock.absoluteFrequency());
			for (auto i : Range(10)) {
				BigInt v = i;
				v |= BigInt(i*13) << 90;
				simu(incrementPin) = v;
				co_await WaitFor(Seconds(5)/clock.absoluteFrequency());
			}
		});
		addSimulationProcess([=]()->SimProcess{

			co_await WaitClk(clock);

			BigInt expectedSum = 0;

			while (true) {
				expectedSum ^= (BigInt) simu(incrementPin);

				BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
				BOOST_TEST(simu(outputPin).allDefined());

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}

struct test_exception_t : std::runtime_error
{
	test_exception_t() : std::runtime_error("Test exception") {}
};

BOOST_FIXTURE_TEST_CASE(SimProc_ExceptionForwarding, BoostUnitTestSimulationFixture)
{
	using namespace gtry;


	Clock clock({ .absoluteFrequency = 1 });


	addSimulationProcess([=]()->SimProcess{
		co_await WaitFor(Seconds(3));
		throw test_exception_t{};
	});


	BOOST_CHECK_THROW(runTicks(clock.getClk(), 10), std::runtime_error);
}


BOOST_FIXTURE_TEST_CASE(SimProc_PingPong, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Clock clock({ .absoluteFrequency = 10'000 });
	{
		auto A_in = pinIn(8_b);
		auto A_out = pinOut(A_in);

		auto B_in = pinIn(8_b);
		auto B_out = pinOut(B_in);

		addSimulationProcess([=]()->SimProcess{
			unsigned i = 0;
			while (true) {
				simu(A_in) = i;
				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
				BOOST_TEST(simu(B_out) == i);
				i++;
			}
		});
		addSimulationProcess([=]()->SimProcess{

			co_await WaitFor(Seconds(1,2)/clock.absoluteFrequency());

			while (true) {
				simu(B_in) = simu(A_out);

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 10);
}



BOOST_FIXTURE_TEST_CASE(SimProc_AsyncProcs, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	{
		UInt a = 8_b;
		UInt b = 8_b;
		UInt sum = pinIn(8_b);
		HCL_NAMED(sum);
		HCL_NAMED(a);
		HCL_NAMED(b);
		pinOut(a);
		pinOut(b);

		a = pinIn(8_b);
		b = pinIn(8_b);
		pinOut(sum);

		addSimulationProcess([=]()->SimProcess{
			while (true) {
				ReadSignalList allInputs;

				simu(sum) = simu(a) + simu(b);

				co_await allInputs.anyInputChange();
			}
		});
		addSimulationProcess([=]()->SimProcess{

			co_await WaitFor(Seconds(1,2)/clock.absoluteFrequency());

			std::mt19937 rng(1337);
			std::uniform_int_distribution<unsigned> dist(0, 100);

			size_t x = dist(rng);
			size_t y = dist(rng);
			size_t z = x+y;
			simu(a) = x;
			simu(b) = y;
			co_await WaitFor(0);
			BOOST_TEST(simu(sum) == z);

			while (true) {

				simu(a) = x = dist(rng);
				BOOST_TEST(simu(sum) == z); // still previous value;
				co_await WaitFor(0);
				z = x+y;
				BOOST_TEST(simu(sum) == z); // updated value;

				simu(b) = y = dist(rng);
				BOOST_TEST(simu(sum) == z); // still previous value;
				co_await WaitFor(0);
				z = x+y;
				BOOST_TEST(simu(sum) == z); // updated value;

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 100);
}



/*

BOOST_FIXTURE_TEST_CASE(SimProc_spawnSubTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;


	auto generatePattern = [](const Clock &clock, Bit &bit, char data)->SimProcess{
		// Start bit
		simu(bit) = '0';
		co_await WaitClk(clock);
		// Data bits
		for (auto i : utils::Range(8)) {
			simu(bit) = (bool)(data & (1 << i));
			co_await WaitClk(clock);
		}
		// End bit
		simu(bit) = '1';
		co_await WaitClk(clock);
	}

	Bit line = pinIn();

	addSimulationProcess([=]()->SimProcess{
		simu(line) = '1';
		co_await WaitClk(clock);
		co_await WaitClk(clock);
		auto subTask = runInParallel(generatePattern());

		// Do some other stuff

		subTask.waitFor();

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100);
}
*/



BOOST_FIXTURE_TEST_CASE(SimProc_registerOverride, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	{
		ClockScope clkScp(clock);

		UInt loop(8_b);
		loop = reg(loop, 0);
		auto outputPin = pinOut(loop);

		addSimulationProcess([=]()->SimProcess{

			co_await WaitClk(clock);
			co_await WaitClk(clock);
			co_await WaitClk(clock);
			BOOST_TEST(0 == simu(outputPin));
			co_await WaitClk(clock);
			BOOST_TEST(0 == simu(outputPin));
			co_await WaitClk(clock);
			BOOST_TEST(0 == simu(outputPin));

			simu(loop).drivingReg() = 10;

			BOOST_TEST(10 == simu(outputPin));
			co_await WaitClk(clock);
			BOOST_TEST(10 == simu(outputPin));

			simu(loop).drivingReg() = 20;

			BOOST_TEST(20 == simu(outputPin));
			co_await WaitClk(clock);
			BOOST_TEST(20 == simu(outputPin));

			co_await WaitClk(clock);
			co_await WaitClk(clock);

			stopTest();
		});
	}


	design.postprocess();
	runTest(hlim::ClockRational(1000, 1) / clock.absoluteFrequency());
}
