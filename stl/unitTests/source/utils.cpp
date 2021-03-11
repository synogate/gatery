#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


#include <hcl/stl/utils/BitCount.h>
#include <hcl/stl/utils/OneHot.h>
#include <hcl/utils/BitManipulation.h>



#include <hcl/frontend.h>
#include <hcl/utils.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>


using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, BitCountTest, data::xrange(255) * data::xrange(1, 8), val, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(val, bitsize);
    BVec count = hcl::stl::bitcount(a);
    
    unsigned actualBitCount = hcl::utils::popcount(unsigned(val) & (0xFF >> (8-bitsize)));
    
    BOOST_REQUIRE(count.getWidth() >= (size_t)hcl::utils::Log2(bitsize)+1);
    //sim_debug() << "The bitcount of " << a << " should be " << actualBitCount << " and is " << count;
    sim_assert(count == ConstBVec(actualBitCount, count.getWidth())) << "The bitcount of " << a << " should be " << actualBitCount << " but is " << count;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, Decoder, data::xrange(3), val)
{
    using namespace hcl::core::frontend;
    using namespace hcl::stl;

    DesignScope design;

    OneHot result = decoder(ConstBVec(val, 2));
    BOOST_CHECK(result.size() == 4);
    sim_assert(result == (1u << val)) << "decoded to " << result;

    BVec back = encoder(result);
    BOOST_CHECK(back.size() == 2);
    sim_assert(back == val) << "encoded to " << back;

    EncoderResult prio = priorityEncoder(result);
    BOOST_CHECK(prio.index.size() == 2);
    sim_assert(prio.valid);
    sim_assert(prio.index == val) << "encoded to " << prio.index;

    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ListEncoder, data::xrange(3), val)
{
    using namespace hcl::core::frontend;
    using namespace hcl::stl;

    DesignScope design;

    OneHot result = decoder(ConstBVec(val, 2));
    BOOST_CHECK(result.size() == 4);
    sim_assert(result == (1u << val)) << "decoded to " << result;

    auto indexList = makeIndexList(result);
    BOOST_CHECK(indexList.size() == result.size());

    for (size_t i = 0; i < indexList.size(); ++i)
    {
        sim_assert(indexList[i].value() == i) << indexList[i].value() << " != " << i;
        sim_assert(*indexList[i].valid == ((size_t)val == i)) << *indexList[i].valid << " != " << ((size_t)val == i);
    }

    auto encoded = priorityEncoder<BVec>(indexList.begin(), indexList.end());
    sim_assert(*encoded.valid);
    sim_assert(encoded.value() == val);

    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, PriorityEncoderTreeTest, data::xrange(65), val)
{
    using namespace hcl::core::frontend;
    using namespace hcl::stl;

    DesignScope design;

    uint64_t testVector = 1ull << val;
    if (val == 54) testVector |= 7;
    if (val == 64) testVector = 0;

    auto res = priorityEncoderTree(ConstBVec(testVector, 64), false);
    
    if (testVector)
    {
        BVec ref = hcl::utils::Log2(testVector & -testVector);
        sim_assert(res.valid & res.index == ref) << "wrong index: " << res.index << " should be " << ref;
    }
    else
    {
        sim_assert(!res.valid) << "wrong valid: " << res.valid;
    }

    eval(design.getCircuit());
}
