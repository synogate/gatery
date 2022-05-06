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

struct TestCompound
{
	gtry::UInt a;
	int b;
};

BOOST_FIXTURE_TEST_CASE(CompoundRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	TestCompound inSignal{
		.a = pinIn(2_b),
		.b = 1
	};

	TestCompound resetSignal{
		.a = "b01",
		.b = 2
	};

	TestCompound outSignal = reg(inSignal);
	pinOut(outSignal.a);
	BOOST_TEST(outSignal.b == 1);

	TestCompound outSignalReset = reg(inSignal, resetSignal);
	pinOut(outSignalReset.a);
	BOOST_TEST(outSignalReset.b == 2);

	addSimulationProcess([=, this]()->SimProcess {

		BOOST_TEST(simu(outSignalReset.a) == 1);

		simu(inSignal.a) = 2;
		co_await WaitClk(clock);
		BOOST_TEST(simu(outSignal.a) == 2);
		BOOST_TEST(simu(outSignalReset.a) == 2);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(ContainerRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	static_assert(!Signal<std::vector<int>>);
	static_assert(Signal<std::vector<TestCompound>>);

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	std::vector<UInt> inSignal{ UInt{pinIn(2_b)}, UInt{pinIn(2_b)} };
	std::vector<UInt> inSignalReset{ UInt{"b00"}, UInt{"b11"} };

	std::vector<UInt> outSignal = reg(inSignal);
	pinOut(outSignal[0]);
	pinOut(outSignal[1]);

	std::vector<UInt> outSignalReset = reg(inSignal, inSignalReset);
	pinOut(outSignalReset[0]);
	pinOut(outSignalReset[1]);

	addSimulationProcess([=, this]()->SimProcess {

		BOOST_TEST(simu(outSignalReset[0]) == 0);
		BOOST_TEST(simu(outSignalReset[1]) == 3);

		simu(inSignal[0]) = 1;
		simu(inSignal[1]) = 2;

		co_await WaitClk(clock);

		BOOST_TEST(simu(outSignal[0]) == 1);
		BOOST_TEST(simu(outSignal[1]) == 2);

		BOOST_TEST(simu(outSignalReset[0]) == 1);
		BOOST_TEST(simu(outSignalReset[1]) == 2);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(ArrayRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	static_assert(!Signal<std::array<int, 2>>);
	static_assert(Signal<std::array<TestCompound, 2>>);

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	std::array<UInt,2> inSignal{ UInt{pinIn(2_b)}, UInt{pinIn(2_b)} };
	std::array<UInt,2> inSignalReset{ UInt{"b00"}, UInt{"b11"} };

	std::array<UInt,2> outSignal = reg(inSignal);
	pinOut(outSignal[0]);
	pinOut(outSignal[1]);

	std::array<UInt,2> outSignalReset = reg(inSignal, inSignalReset);
	pinOut(outSignalReset[0]);
	pinOut(outSignalReset[1]);

	addSimulationProcess([=, this]()->SimProcess {

		BOOST_TEST(simu(outSignalReset[0]) == 0);
		BOOST_TEST(simu(outSignalReset[1]) == 3);

		simu(inSignal[0]) = 1;
		simu(inSignal[1]) = 2;

		co_await WaitClk(clock);

		BOOST_TEST(simu(outSignal[0]) == 1);
		BOOST_TEST(simu(outSignal[1]) == 2);

		BOOST_TEST(simu(outSignalReset[0]) == 1);
		BOOST_TEST(simu(outSignalReset[1]) == 2);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(TupleRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	static_assert(Signal<std::tuple<int, UInt>>);
	static_assert(Signal<std::pair<UInt, int>>);

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	std::tuple<int, UInt> inSignal{ 0, UInt{pinIn(2_b)} };
	std::tuple<int, unsigned> inSignalReset{ 1, 3 };

	std::tuple<int, UInt> outSignal = reg(inSignal);
	pinOut(get<1>(outSignal));

	std::tuple<int, UInt> outSignalReset = reg(inSignal, inSignalReset);
	pinOut(get<1>(outSignalReset));

	addSimulationProcess([=, this]()->SimProcess {

		BOOST_TEST(get<0>(outSignalReset) == 1);
		BOOST_TEST(simu(get<1>(outSignalReset)) == 3);

		simu(get<1>(inSignal)) = 2;

		co_await WaitClk(clock);

		BOOST_TEST(simu(get<1>(outSignal)) == 2);
		BOOST_TEST(simu(get<1>(outSignalReset)) == 2);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest({ 1,1 });
}

BOOST_FIXTURE_TEST_CASE(MapRegister, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	static_assert(Signal<std::map<int, UInt>>);

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	std::map<int, UInt> inSignal;
	inSignal[0] = pinIn(2_b);

	std::map<int, int> inSignalReset = { {0,3} };
	
	std::map<int, UInt> outSignal = reg(inSignal);
	std::map<int, UInt> outSignalReset = reg(inSignal, inSignalReset);
	
	addSimulationProcess([&, this]()->SimProcess {

		simu(inSignal[0]) = 2;
		BOOST_TEST(simu(outSignalReset[0]) == 3);

		co_await WaitClk(clock);

		BOOST_TEST(simu(outSignal[0]) == 2);
		BOOST_TEST(simu(outSignalReset[0]) == 2);

		stopTest();
	});

	design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
	runTest({ 1,1 });
}
