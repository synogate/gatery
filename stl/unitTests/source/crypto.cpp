#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/utils.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <hcl/stl/crypto/sha1.h>

using namespace boost::unit_test;
using namespace hcl;

BOOST_FIXTURE_TEST_CASE(Sha1Rounds, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    BVec round = 0;
    BVec k = 0xCA62C1D6;

    IF(round < 20)
        k = 0x5A827999;
    ELSE IF(round < 40)
        k = 0x6ED9EBA1;
    ELSE IF(round < 60)
        k = 0x8F1BBCDC;

    sim_assert(k == 0x5A827999u) << "k wrong " << k;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<BVec, stl::Adder<>> sha1, sha1ref;
    sha1.beginBlock(msgBlock);
    sim_assert(sha1.w[0] == "x80000000") << "w0";
    
    sha1.round(0);

    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t f = (h1 & h2) | (~h1 & h3);
    //uint32_t k = 0x5A827999;
    //  + ConstBVec(f + k + 0x80000000, 32)
    sim_assert(sha1.a == rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k) << "a wrong";
    sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
    sim_assert(sha1.c == rotl(sha1ref.b, 30));
    sim_assert(sha1.d == sha1ref.c);
    sim_assert(sha1.e == sha1ref.d);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(Sha1, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<> sha1;
    sha1.beginBlock(msgBlock);
    stl::HashEngine<stl::Sha1Generator<>> sha1Engine(0, 0);
    sha1Engine.buildPipeline(sha1);
    sha1.endBlock();

    BVec hash = sha1.finalize();
    BVec ref = "xDA39A3EE5E6B4B0D3255BFEF95601890AFD80709";
    sim_debug() << hash;
    sim_debug() << ref;
    sim_assert(hash(0, 64) == ref(0, 64));
    sim_assert(hash(64, 64) == ref(64, 64));

    eval(design.getCircuit());
}
