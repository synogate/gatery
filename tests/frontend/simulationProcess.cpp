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

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(SimProc_Basics, BoostUnitTestSimulationFixture)
{
    using namespace gtry;



    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000).setResetType(ClockConfig::ResetType::NONE));
    {
        ClockScope clkScp(clock);

        BVec counter(8_b);
        counter = reg(counter, 0);

        auto incrementPin = pinIn(8_b);
        auto outputPin = pinOut(counter);
        counter += incrementPin;

        addSimulationProcess([=]()->SimProcess{
            co_await WaitFor(Seconds(1, 2)/clock.getAbsoluteFrequency());
            for (auto i : Range(10)) {
                simu(incrementPin) = i;
                co_await WaitFor(Seconds(5)/clock.getAbsoluteFrequency());
            }
        });
        addSimulationProcess([=]()->SimProcess{

            co_await WaitClk(clock);

            size_t expectedSum = 0;

            while (true) {
                expectedSum += simu(incrementPin);

                BOOST_TEST(expectedSum == simu(outputPin));
                BOOST_TEST(simu(outputPin).defined() == 0xFF);

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}



BOOST_FIXTURE_TEST_CASE(SimProc_BigInt_small, BoostUnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000).setResetType(ClockConfig::ResetType::NONE));
    {
        ClockScope clkScp(clock);

        BVec counter(40_b);
        counter = reg(counter, 0);

        auto incrementPin = pinIn(40_b);
        auto outputPin = pinOut(counter);
        counter += incrementPin;

        addSimulationProcess([=]()->SimProcess{
            co_await WaitFor(Seconds(1, 2)/clock.getAbsoluteFrequency());
            for (auto i : Range(10)) {
                BigInt v = i;
                v |= BigInt(i*13) << 20;
                simu(incrementPin) = v;
                co_await WaitFor(Seconds(5)/clock.getAbsoluteFrequency());
            }
        });
        addSimulationProcess([=]()->SimProcess{

            co_await WaitClk(clock);

            BigInt expectedSum = 0;

            while (true) {
                expectedSum += (BigInt) simu(incrementPin);

                BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
                BOOST_TEST(simu(outputPin).allDefined());

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}



BOOST_FIXTURE_TEST_CASE(SimProc_BigInt, BoostUnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000).setResetType(ClockConfig::ResetType::NONE));
    {
        ClockScope clkScp(clock);

        BVec counter(128_b);
        counter = reg(counter, 0);

        auto incrementPin = pinIn(128_b);
        auto outputPin = pinOut(counter);
        counter ^= incrementPin;

        addSimulationProcess([=]()->SimProcess{
            co_await WaitFor(Seconds(1, 2)/clock.getAbsoluteFrequency());
            for (auto i : Range(10)) {
                BigInt v = i;
                v |= BigInt(i*13) << 90;
                simu(incrementPin) = v;
                co_await WaitFor(Seconds(5)/clock.getAbsoluteFrequency());
            }
        });
        addSimulationProcess([=]()->SimProcess{

            co_await WaitClk(clock);

            BigInt expectedSum = 0;

            while (true) {
                expectedSum ^= (BigInt) simu(incrementPin);

                BOOST_TEST((expectedSum == (BigInt) simu(outputPin)));
                BOOST_TEST(simu(outputPin).allDefined());

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}

struct test_exception_t : std::runtime_error
{
    test_exception_t() : std::runtime_error("Test exception") {}
};

BOOST_FIXTURE_TEST_CASE(SimProc_ExceptionForwarding, BoostUnitTestSimulationFixture)
{
    using namespace gtry;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(1));


    addSimulationProcess([=]()->SimProcess{
        co_await WaitFor(Seconds(3));
        throw test_exception_t{};
    });


    BOOST_CHECK_THROW(runTicks(clock.getClk(), 10), std::runtime_error);
}


BOOST_FIXTURE_TEST_CASE(SimProc_PingPong, BoostUnitTestSimulationFixture)
{
    using namespace gtry;



    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    {
        auto A_in = pinIn(8_b);
        auto A_out = pinOut(A_in);

        auto B_in = pinIn(8_b);
        auto B_out = pinOut(B_in);

        addSimulationProcess([=]()->SimProcess{
            unsigned i = 0;
            while (true) {
                simu(A_in) = i;
                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
                BOOST_TEST(simu(B_out) == i);
                i++;
            }
        });
        addSimulationProcess([=]()->SimProcess{

            co_await WaitFor(Seconds(1,2)/clock.getAbsoluteFrequency());

            while (true) {
                simu(B_in) = simu(A_out);

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 10);
}
