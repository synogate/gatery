#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/stl/io/HDMITransmitter.h>
#include <hcl/stl/utils/BitCount.h>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>


using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, tmdsReduction, data::xrange(255), val)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    auto a = ConstBVec(val, 8);

    BVec encoded = hcl::stl::hdmi::tmdsEncodeReduceTransitions(a);
    BOOST_TEST(encoded.getWidth() == a.getWidth() + 1);

    BVec decoded = hcl::stl::hdmi::tmdsDecodeReduceTransitions(encoded);
    sim_assert(a == decoded);
    sim_debug() << a << " => " << encoded << " => " << decoded << " | " << hcl::stl::bitcount(a);
    ;

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(tmdsBitflip, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);
    
    Register<BVec> test_counter(8, Expansion::none);
    test_counter.setReset("8b0");
    test_counter += 1;

    BVec encoded = hcl::stl::hdmi::tmdsEncodeBitflip(clock, test_counter.delay(1));
    BOOST_TEST(test_counter.getWidth() == encoded.getWidth() - 1);

    BVec decoded = hcl::stl::hdmi::tmdsDecodeBitflip(encoded);
    sim_assert(decoded == test_counter.delay(1));

    runTicks(design.getCircuit(), clock.getClk(), 260);
}

