#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <iostream>

using namespace boost::unit_test;
using namespace hcl::core::frontend;
using namespace hcl::utils;

BOOST_FIXTURE_TEST_CASE(SimProc_Basics, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    {
        ClockScope clkScp(clock);

        BVec counter(8_b);
        counter = reg(counter, 0);

        auto incrementPin = pinIn(8_b);
        auto outputPin = pinOut(counter);
        counter += incrementPin;

        addSimulationProcess([&]()->SimProcess{
            for (auto i : Range(10)) {
                sim(incrementPin) = i;
                co_await WaitFor(Seconds(5)/clock.getAbsoluteFrequency());
            }
        });
        addSimulationProcess([&]()->SimProcess{

            co_await WaitClk(clock);

            unsigned expectedSum = 0;

            while (true) {
                expectedSum += sim(incrementPin);

                BOOST_TEST(expectedSum == sim(outputPin));
                BOOST_TEST(sim(outputPin).defined() == 0xFF);

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().optimize(3);
    runTicks(design.getCircuit(), clock.getClk(), 5*10 + 3); // do some extra rounds without generator changing the input
}


BOOST_FIXTURE_TEST_CASE(SimProc_ExceptionForwarding, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;
    Clock clock(ClockConfig{}.setAbsoluteFrequency(1));


    addSimulationProcess([&]()->SimProcess{
        co_await WaitFor(Seconds(3));
        throw std::runtime_error("Test exception");
    });


    BOOST_CHECK_THROW(runTicks(design.getCircuit(), clock.getClk(), 10), std::runtime_error);
}


BOOST_FIXTURE_TEST_CASE(SimProc_PingPong, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    {
        auto A_in = pinIn(8_b);
        auto A_out = pinOut(A_in);

        auto B_in = pinIn(8_b);
        auto B_out = pinOut(B_in);

        addSimulationProcess([&]()->SimProcess{
            unsigned i = 0;
            while (true) {
                sim(A_in) = i;
                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
                BOOST_TEST(sim(B_out) == i);
                i++;
            }
        });
        addSimulationProcess([&]()->SimProcess{

            co_await WaitFor(Seconds(1,2)/clock.getAbsoluteFrequency());

            while (true) {
                sim(B_in) = sim(A_out);

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().optimize(3);
    runTicks(design.getCircuit(), clock.getClk(), 10);
}