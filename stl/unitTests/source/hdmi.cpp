#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/stl/io/HDMITransmitter.h>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>


using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, tmdsReduction, data::xrange(255), val)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    auto a = ConstBitVector(val, 8);

    BitVector encoded = hcl::stl::hdmi::tmdsEncodeReduceTransitions(a);
    BOOST_TEST(encoded.getWidth() == a.getWidth() + 1);

    BitVector decoded = hcl::stl::hdmi::tmdsDecodeReduceTransitions(encoded);
    sim_assert(a == decoded);

    eval(design.getCircuit());
}


