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

#include <gatery/simulation/Simulator.h>
#include <gatery/simulation/simProc/SimulationFiber.h>

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

			size_t expectedSum = 0;

			while (true) {
				co_await AfterClk(clock);

				expectedSum += simu(incrementPin);

				BOOST_TEST(expectedSum == simu(outputPin));
				BOOST_TEST(simu(outputPin).defined() == 0xFF);
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

			BigInt expectedSum = 0;

			while (true) {
				co_await AfterClk(clock);
				expectedSum += (BigInt) simu(incrementPin);

				BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
				BOOST_TEST(simu(outputPin).allDefined());
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

			BigInt expectedSum = 0;

			while (true) {
				co_await AfterClk(clock);
				expectedSum ^= (BigInt) simu(incrementPin);

				BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
				BOOST_TEST(simu(outputPin).allDefined());
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
			co_await WaitStable();
			BOOST_TEST(simu(sum) == z);
			co_await WaitFor(Seconds(1)/clock.absoluteFrequency());

			while (true) {

				simu(a) = x = dist(rng);
				BOOST_TEST(simu(sum) == z); // still previous value;
				co_await WaitStable();
				z = x+y;
				BOOST_TEST(simu(sum) == z); // updated value;

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());

				simu(b) = y = dist(rng);
				BOOST_TEST(simu(sum) == z); // still previous value;
				co_await WaitStable();
				z = x+y;
				BOOST_TEST(simu(sum) == z); // updated value;

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 100);
}


unsigned depth = 0;

class StackDepthCounter {
	public:
		StackDepthCounter() { depth++; }
		~StackDepthCounter() { depth--; }
};

BOOST_FIXTURE_TEST_CASE(SimProc_callSubTaskStackOverflowTest, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });


	auto subProcess = []()->SimProcess{
		volatile char stackFiller[1024]; stackFiller;
		StackDepthCounter counter;
		co_return;
	};

	addSimulationProcess([=, this]()->SimProcess{
		for ([[maybe_unused]] auto i : gtry::utils::Range(1'000'000)) {
			co_await subProcess();
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}

BOOST_FIXTURE_TEST_CASE(SimProc_callSuspendingSubTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool busy = false;
	auto subProcess = [&busy](const Clock &clock)->SimProcess{
		busy = true;
		for ([[maybe_unused]] auto i : gtry::utils::Range(8)) {
			co_await AfterClk(clock);
		}
		busy = false;
	};

	addSimulationProcess([=, this]()->SimProcess{
		co_await AfterClk(clock);
		for ([[maybe_unused]] auto i : gtry::utils::Range(1000)) {
			co_await AfterClk(clock);
			BOOST_TEST(!busy);
			co_await subProcess(clock);
			BOOST_TEST(!busy);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(SimProc_callReturnValueTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	int i = 0;
	auto subProcess = [&i](const Clock &clock)->SimFunction<int>{
		co_await AfterClk(clock);
		co_return i;
	};

	addSimulationProcess([&i,this,clock, subProcess]()->SimProcess{
		co_await AfterClk(clock);
		
		i = 5;
		BOOST_TEST(co_await subProcess(clock) == 5);

		i = 6;
		BOOST_TEST(co_await subProcess(clock) == 6);

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(SimProc_forkTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool flag = false;
	auto subProcess = [&flag](const Clock &clock)->SimProcess{
		flag = true;
		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			co_await AfterClk(clock);
			flag = !flag;
		}
	};

	addSimulationProcess([&flag,clock,subProcess,this]()->SimProcess{
		BOOST_TEST(!flag);
		co_await AfterClk(clock);
		fork(subProcess(clock)); // fire & forget
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
			BOOST_TEST(!flag);
			co_await AfterClk(clock);
		}

		// Tasks should have finished by now
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(SimProc_forkUnendingTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool flag = false;
	auto subProcess = [&flag](const Clock &clock)->SimProcess{
		flag = true;
		while (true) {
			co_await AfterClk(clock);
			flag = !flag;
		}
	};

	addSimulationProcess([&flag,clock,subProcess,this]()->SimProcess{
		BOOST_TEST(!flag);
		co_await AfterClk(clock);
		fork(subProcess(clock)); // fire & forget
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
			BOOST_TEST(!flag);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(SimProc_forkFromSimProc, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool flag = false;

	auto subProcess2 = [&flag](const Clock &clock)->SimProcess {
		fork([&flag, &clock]()->SimProcess {
			flag = true;
			while (true) {
				co_await AfterClk(clock);
				flag = !flag;
			}
		});
		co_return;
	};

	addSimulationProcess([&flag,clock,subProcess2,this]()->SimProcess {
		BOOST_TEST(!flag);
		co_await AfterClk(clock);
		co_await subProcess2(clock);
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
			BOOST_TEST(!flag);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}

namespace {

SimProcess unsuspending() {
	co_return;
}

}

BOOST_FIXTURE_TEST_CASE(SimProc_forkUnsuspendingCoro, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });



	addSimulationProcess([clock,this]()->SimProcess {
		co_await AfterClk(clock);

		co_await unsuspending();

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(SimProc_joinTask, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool flag = false;
	auto subProcess = [&flag](const Clock &clock)->SimProcess{
		flag = true;
		for ([[maybe_unused]] auto i : gtry::utils::Range(100)) {
			co_await AfterClk(clock);
			flag = !flag;
		}
	};

	addSimulationProcess([&flag,clock,subProcess,this]()->SimProcess{
		BOOST_TEST(!flag);
		co_await AfterClk(clock);
		
		auto task = fork(subProcess(clock));

		for ([[maybe_unused]] auto i : gtry::utils::Range(5)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
			BOOST_TEST(!flag);
			co_await AfterClk(clock);
		}

		co_await join(task);

		// Tasks should have finished by now
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			BOOST_TEST(flag);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}




BOOST_FIXTURE_TEST_CASE(SimProc_condition, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	bool resourceInUse = false;
	Seconds timeResourceReleased = 0;
	size_t microTickResourceReleased = 0;
	Condition condition;

	auto subProcess = [&resourceInUse,&timeResourceReleased,&microTickResourceReleased,&condition,this](const Clock &clock)->SimProcess{
		while (true) {
			// Suspend until the resource is available
			while (resourceInUse)
				co_await condition.wait();

			// Resource is available

			// Check that we got here as soon as it got available (e.g. without loosing simulation time)
			HCL_ASSERT(m_simulator->getCurrentSimulationTime() == timeResourceReleased);
			HCL_ASSERT(m_simulator->getCurrentMicroTick() == microTickResourceReleased);

			// Use resource (for one clock cycle)
			resourceInUse = true;
			co_await OnClk(clock);
			// Release resource and notify suspended coroutines
			resourceInUse = false;
			timeResourceReleased = m_simulator->getCurrentSimulationTime();
			microTickResourceReleased = m_simulator->getCurrentMicroTick();
			condition.notify_one();

			// Spend a clock cycle not using the resource
			co_await OnClk(clock);
		}
	};

	addSimulationProcess([&resourceInUse,clock,subProcess,this]()->SimProcess{
		BOOST_TEST(!resourceInUse);

		fork(subProcess(clock)); // fire & forget
		fork(subProcess(clock)); // fire & forget


		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await OnClk(clock);
			BOOST_TEST(resourceInUse);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}



BOOST_FIXTURE_TEST_CASE(SimProc_registerOverride, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	{
		ClockScope clkScp(clock);

		UInt loop(8_b);
		loop = reg(loop, 0);
		auto outputPin = pinOut(loop);

		addSimulationProcess([=, this]()->SimProcess{

			co_await AfterClk(clock);
			co_await AfterClk(clock);
			co_await AfterClk(clock);
			BOOST_TEST(0 == simu(outputPin));
			co_await AfterClk(clock);
			BOOST_TEST(0 == simu(outputPin));
			co_await AfterClk(clock);
			BOOST_TEST(0 == simu(outputPin));

			simu(loop).drivingReg() = 10;

			BOOST_TEST(10 == simu(outputPin));
			co_await AfterClk(clock);
			BOOST_TEST(10 == simu(outputPin));

			simu(loop).drivingReg() = 20;

			BOOST_TEST(20 == simu(outputPin));
			co_await AfterClk(clock);
			BOOST_TEST(20 == simu(outputPin));

			co_await AfterClk(clock);
			co_await AfterClk(clock);

			stopTest();
		});
	}


	design.postprocess();
	runTest(hlim::ClockRational(1000, 1) / clock.absoluteFrequency());
}

namespace {

struct TestStruct {
	std::vector<char> data;
	TestStruct() {
		//std::cout << "CTOR of " << (size_t) this << std::endl;
	}
	TestStruct(const TestStruct &rhs) {
		//std::cout << "C-CTOR " << (size_t) this << " <-- " << (size_t) &rhs << std::endl;
		data = rhs.data;
		//std::cout << "  data:" << (size_t) data.data() << " <-- " << (size_t) rhs.data.data() << std::endl;
	}

	TestStruct(const TestStruct &&rhs) = delete;
	void operator=(const TestStruct &rhs) = delete;
	void operator=(const TestStruct &&rhs) = delete;

	~TestStruct() {
		//std::cout << "DTOR of " << (size_t) this << std::endl;
		//std::cout << "  data:" << (size_t) data.data() << std::endl;
	}
};


struct TestStruct2 {
	TestStruct2() {
		//std::cout << "2__CTOR of " << (size_t) this << std::endl;
	}
	TestStruct2(const TestStruct2 &rhs) {
		//std::cout << "2__C-CTOR " << (size_t) this << " <-- " << (size_t) &rhs << std::endl;
	}

	TestStruct2(const TestStruct2 &&rhs) = delete;
	void operator=(const TestStruct2 &rhs) = delete;
	void operator=(const TestStruct2 &&rhs) = delete;

	~TestStruct2() {
		//std::cout << "2__DTOR of " << (size_t) this << std::endl;
	}
};

}

BOOST_FIXTURE_TEST_CASE(SimProc_copyCaptureLambda, BoostUnitTestSimulationFixture, * boost::unit_test::disabled())
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	addSimulationProcess([clock,this]()->SimProcess {

		TestStruct test;
		test.data.resize(10);

		/*
			This triggers a bug in gcc 10 and 11 which results in a double free
		*/

		co_await [=,teststruct2 = TestStruct2{}]() -> SimProcess {
			for ([[maybe_unused]] auto &v : test.data) { }
			co_return;
		}();

		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}

BOOST_FIXTURE_TEST_CASE(SimProc_copyCaptureLambdaFork, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	addSimulationProcess([clock,this]()->SimProcess {

		TestStruct test;
		test.data.resize(10);
		
		auto handle = fork([=,teststruct2 = TestStruct2{}]() -> SimProcess {
			for ([[maybe_unused]] auto &v : test.data) { }
			co_return;
		});

		co_await join(handle);

		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await AfterClk(clock);
		}

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(simu_assign_vector, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	UInt largeInput = pinIn(128_b).setName("largeInput");

	UInt firstByte = largeInput.lower(8_b);
	UInt lastByte = largeInput.upper(8_b);
	pinOut(firstByte);
	pinOut(lastByte);

	addSimulationProcess([=,this]()->SimProcess {

		BOOST_TEST(!simu(firstByte).allDefined());
		BOOST_TEST(!simu(lastByte).allDefined());

		co_await AfterClk(clock);

		std::vector<uint8_t> byteInput(16);
		for (auto i : Range(byteInput.size()))
			byteInput[i] = uint8_t(42 + i);
		simu(largeInput) = std::span{byteInput};

		co_await AfterClk(clock);

		BOOST_TEST(simu(firstByte) == 42);
		BOOST_TEST(simu(lastByte) == 42+15);

		co_await AfterClk(clock);

		std::vector<uint64_t> wordInput(2);
		for (auto i : Range(wordInput.size())) wordInput[i] = (42+i) | ((uint64_t)(13+i*5) << 7*8);
		simu(largeInput) = std::span{wordInput};

		co_await AfterClk(clock);

		BOOST_TEST(simu(firstByte) == 42);
		BOOST_TEST(simu(lastByte) == 13+5);

		co_await AfterClk(clock);

		simu(largeInput) = byteInput;

		co_await AfterClk(clock);

		BOOST_TEST(simu(firstByte) == 42);
		BOOST_TEST(simu(lastByte) == 42+15);

		co_await AfterClk(clock);

		simu(largeInput) = wordInput;

		co_await AfterClk(clock);

		BOOST_TEST(simu(firstByte) == 42);
		BOOST_TEST(simu(lastByte) == 13+5);

		co_await AfterClk(clock);


		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}


BOOST_FIXTURE_TEST_CASE(simu_compare_vector, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	UInt largeInput = pinIn(128_b).setName("largeInput");

	UInt middleWord = largeInput(32, 64_b);
	pinOut(middleWord);

	addSimulationProcess([=,this]()->SimProcess {

		BOOST_TEST(!simu(middleWord).allDefined());

		co_await AfterClk(clock);

		std::vector<uint8_t> byteInput(16);
		for (auto i : Range(byteInput.size()))
			byteInput[i] = uint8_t(42 + i);
		auto middleByteSpan = std::span<uint8_t>(byteInput.begin() + 4, 8);

		BOOST_TEST(simu(middleWord) != middleByteSpan);
		
		simu(largeInput) = byteInput;

		co_await AfterClk(clock);

		BOOST_TEST(simu(middleWord) == middleByteSpan);

		for (auto i : Range(byteInput.size())) byteInput[i] = 0;
		
		BOOST_TEST(simu(middleWord) != middleByteSpan);

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}

BOOST_FIXTURE_TEST_CASE(simu_read_vector, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	UInt largeInput = pinIn(128_b).setName("largeInput");

	UInt middleWord = largeInput(32, 64_b);
	pinOut(middleWord);

	addSimulationProcess([=,this]()->SimProcess {

		BOOST_TEST(!simu(middleWord).allDefined());

		co_await AfterClk(clock);

		std::vector<uint8_t> byteInput(16);
		for (auto i : Range(byteInput.size()))
			byteInput[i] = uint8_t(42 + i);
		auto middleByteSpan = std::span<uint8_t>(byteInput.begin() + 4, 8);
		simu(largeInput) = byteInput;

		co_await AfterClk(clock);
		
		// Explicit
		auto middleVector = simu(middleWord).toVector<uint8_t>();

		BOOST_REQUIRE(middleVector.size() == middleByteSpan.size());
		for (auto i : Range(middleVector.size())) 
			BOOST_TEST(middleVector[i] == middleByteSpan[i]);

		co_await AfterClk(clock);

		for (auto i : Range(byteInput.size()))
			byteInput[i] = uint8_t(13 + i * 5);
		simu(largeInput) = byteInput;

		co_await AfterClk(clock);

		// Implicit
		middleVector = simu(middleWord);

		BOOST_REQUIRE(middleVector.size() == middleByteSpan.size());
		for (auto i : Range(middleVector.size())) 
			BOOST_TEST(middleVector[i] == middleByteSpan[i]);

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}

BOOST_FIXTURE_TEST_CASE(simu_read_large_vector, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });

	UInt largeInput = pinIn(128_b).setName("largeInput");

	UInt middleWord = largeInput(32, 64_b);
	pinOut(middleWord);

	addSimulationProcess([=,this]()->SimProcess {

		BOOST_TEST(!simu(middleWord).allDefined());

		co_await AfterClk(clock);

		std::vector<uint8_t> byteInput(16);
		for (auto i : Range(byteInput.size()))
			byteInput[i] = uint8_t(42 + i);
		auto middleByteSpan = std::span<uint8_t>(byteInput.begin() + 4, 8);
		simu(largeInput) = byteInput;

		co_await AfterClk(clock);
		
		// Explicit
		auto middleVector = simu(middleWord).toVector<uint32_t>();

		BOOST_REQUIRE(middleVector.size()*4 == middleByteSpan.size());
		for (auto i : Range(middleVector.size())) 
			BOOST_TEST(middleVector[i] == (middleByteSpan[i*4+0]
										    | (middleByteSpan[i*4+1] << 8)
										    | (middleByteSpan[i*4+2] << 16)
										    | (middleByteSpan[i*4+3] << 24)) );

		stopTest();
	});

	design.postprocess();

	runTicks(clock.getClk(), 100000);
}



BOOST_FIXTURE_TEST_CASE(SimFiber_PingPong, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	Clock clock({ .absoluteFrequency = 10'000 });
	{
		auto A_in = pinIn(8_b);
		auto A_out = pinOut(A_in);

		auto B_in = pinIn(8_b);
		auto B_out = pinOut(B_in);

		addSimulationFiber([=](){
			unsigned i = 0;
			while (true) {
				auto b = sim::SimulationFiber::awaitCoroutine<size_t>([=]()->SimFunction<size_t> {
					simu(A_in) = i;
					co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
					co_return (size_t) simu(B_out);
				});
				BOOST_TEST(b == i);
				i++;
			}
		});

		addSimulationFiber([=]()->SimProcess{

			sim::SimulationFiber::awaitCoroutine<size_t>([=]()->SimFunction<size_t> {
				co_await WaitFor(Seconds(1,2)/clock.absoluteFrequency());
				co_return 0;
			});

			while (true) {
				sim::SimulationFiber::awaitCoroutine<size_t>([=]()->SimFunction<size_t> {
					simu(B_in) = simu(A_out);

					co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
					co_return 0;
				});
			}
		});
	}


	design.postprocess();
	runTicks(clock.getClk(), 10);
}


/*

BOOST_FIXTURE_TEST_CASE(SimFromThread, BoostUnitTestSimulationFixture)
{
	using namespace gtry;


	Clock clock({ .absoluteFrequency = 10'000 });

	std::function<void()> threadBody;
	{
		auto A_in = pinIn(8_b);
		auto A_out = pinOut(A_in);

		auto B_in = pinIn(8_b);
		auto B_out = pinOut(B_in);

		threadBody = [=, this](){
			unsigned i = 0;
			while (i < 100) {
				auto b = m_simulator->executeCoroutine<size_t>([=]()->SimFunction<size_t> {
					simu(A_in) = i;
					co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
					co_return (size_t) simu(B_out);
				});
				BOOST_TEST(b == i);
				i++;
			}
		};

		addSimulationProcess([=]()->SimProcess {

			co_await WaitFor(Seconds(1,2)/clock.absoluteFrequency());

			while (true) {
				simu(B_in) = simu(A_out);

				co_await WaitFor(Seconds(1)/clock.absoluteFrequency());
			}
		});
	}


	design.postprocess();

	m_simulator->compileProgram(design.getCircuit());
	m_simulator->powerOn();

	std::thread thread(threadBody);
	thread.join();

}

*/

