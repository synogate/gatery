#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include <hcl/stl/utils/BitCount.h>
#include <hcl/utils/BitManipulation.h>



#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <bit>

using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, BitCountTest, data::xrange(255) * data::xrange(1, 8), val, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(val, bitsize);
    UnsignedInteger count = hcl::stl::bitcount(a);
    
    unsigned actualBitCount = std::popcount(unsigned(val) & (0xFF >> (8-bitsize)));
    
    BOOST_REQUIRE(count.getWidth() >= hcl::utils::truncLog2(bitsize)+1);
    //sim_debug() << "The bitcount of " << a << " should be " << actualBitCount << " and is " << count;
    sim_assert(count == ConstUnsignedInteger(actualBitCount, count.getWidth())) << "The bitcount of " << a << " should be " << actualBitCount << " but is " << count;
    
    eval(design.getCircuit());
}


