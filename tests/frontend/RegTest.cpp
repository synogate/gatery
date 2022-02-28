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
    gtry::BVec a;
    int b;
};

BOOST_FIXTURE_TEST_CASE(CompoundRegister, gtry::BoostUnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
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

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    std::vector<BVec> inSignal{ BVec{pinIn(2_b)}, BVec{pinIn(2_b)} };
    std::vector<BVec> inSignalReset{ BVec{"b00"}, BVec{"b11"} };

    std::vector<BVec> outSignal = reg(inSignal);
    pinOut(outSignal[0]);
    pinOut(outSignal[1]);

    std::vector<BVec> outSignalReset = reg(inSignal, inSignalReset);
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

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    std::array<BVec,2> inSignal{ BVec{pinIn(2_b)}, BVec{pinIn(2_b)} };
    std::array<BVec,2> inSignalReset{ BVec{"b00"}, BVec{"b11"} };

    std::array<BVec,2> outSignal = reg(inSignal);
    pinOut(outSignal[0]);
    pinOut(outSignal[1]);

    std::array<BVec,2> outSignalReset = reg(inSignal, inSignalReset);
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
