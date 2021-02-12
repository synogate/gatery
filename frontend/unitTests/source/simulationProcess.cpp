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

            for (auto i : Range(10)) {
                expectedSum += sim(incrementPin);

                BOOST_TEST(expectedSum == sim(outputPin));
                BOOST_TEST(sim(outputPin).defined() == 0xFF);

                co_await WaitFor(Seconds(1)/clock.getAbsoluteFrequency());
            }
        });
    }


    design.getCircuit().optimize(3);
    runTicks(design.getCircuit(), clock.getClk(), 5*10);
}
