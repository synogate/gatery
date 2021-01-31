#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/utils.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <hcl/stl/crypto/sha1.h>
#include <hcl/stl/crypto/md5.h>

using namespace boost::unit_test;
using namespace hcl;

BOOST_FIXTURE_TEST_CASE(Sha1RoundA, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<> sha1, sha1ref;
    sha1.beginBlock(msgBlock);
    sim_assert(sha1.w[0] == "x80000000") << "w0";
    
    sha1.round(0);

    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t f = (h1 & h2) | (~h1 & h3);
    uint32_t k = 0x5A827999;

    BVec checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
    sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
    sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
    sim_assert(sha1.c == rotl(sha1ref.b, 30));
    sim_assert(sha1.d == sha1ref.c);
    sim_assert(sha1.e == sha1ref.d);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundB, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<> sha1, sha1ref;
    sha1.beginBlock(msgBlock);
    sim_assert(sha1.w[0] == "x80000000") << "w0";

    sha1.round(20);

    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t f = h1 ^ h2 ^ h3;
    uint32_t k = 0x6ED9EBA1;

    BVec checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
    sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
    sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
    sim_assert(sha1.c == rotl(sha1ref.b, 30));
    sim_assert(sha1.d == sha1ref.c);
    sim_assert(sha1.e == sha1ref.d);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundC, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<> sha1, sha1ref;
    sha1.beginBlock(msgBlock);
    sim_assert(sha1.w[0] == "x80000000") << "w0";

    sha1.round(40);

    uint32_t b = 0xEFCDAB89;
    uint32_t c = 0x98BADCFE;
    uint32_t d = 0x10325476;
    uint32_t f = (b & c) | (b & d) | (c & d);
    uint32_t k = 0x8F1BBCDC;

    BVec checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
    sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
    sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
    sim_assert(sha1.c == rotl(sha1ref.b, 30));
    sim_assert(sha1.d == sha1ref.c);
    sim_assert(sha1.e == sha1ref.d);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundD, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';

    stl::Sha1Generator<> sha1, sha1ref;
    sha1.beginBlock(msgBlock);
    sim_assert(sha1.w[0] == "x80000000") << "w0";

    sha1.round(60);

    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t f = h1 ^ h2 ^ h3;
    uint32_t k = 0xCA62C1D6;

    BVec checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
    sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
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

    sim_assert(hash(0, 64) == ref(0, 64)); // TODO: implement large compare simulation
    sim_assert(hash(64, 64) == ref(64, 64));
    sim_assert(hash(128, 32) == ref(128, 32));

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(Md5, hcl::core::sim::UnitTestSimulationFixture)
{
    struct md5ref
    {
        uint32_t a = 0x67452301;
        uint32_t b = 0xefcdab89;
        uint32_t c = 0x98badcfe;
        uint32_t d = 0x10325476;

        uint32_t w[16] = { 0x80 };

        md5ref round(uint32_t idx) const
        {
            md5ref ret;

            const uint32_t K = uint32_t(pow(2., 32.) * abs(sin(idx + 1)));

            const uint32_t S_table[4][4] = { {7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21} };
            const uint32_t S = S_table[idx / 16][idx % 4];

            const uint32_t Wmul[4] = { 1, 5, 3, 7 };
            const uint32_t Wadd[4] = { 0, 1, 5, 0 };
            const uint32_t W = Wmul[idx / 16] * idx + Wadd[idx / 16];

            uint32_t F;
            switch (idx / 16) {
            case 0: F = (b & c) | (~b & d); break;
            case 1: F = (d & b) | (~d & c); break;
            case 2: F = b ^ c ^ d;          break;
            default:F = c ^ (b | ~d);
            }

            uint32_t tmp = F + a + K + w[W % 16];
            tmp = (tmp << S) | (tmp >> (32 - S));

            ret.a = d;
            ret.b = tmp + b;
            ret.c = b;
            ret.d = c;
            return ret;
        }
    };

    DesignScope design;

    // create padded empty input
    BVec msgBlock = "512x0";
    msgBlock.msb() = '1';
    stl::Md5Generator<> md5;
    md5.beginBlock(msgBlock);

    md5ref refImpl;
    for (size_t i = 0; i < 64; ++i)
    {
        md5.round(i);
        refImpl = refImpl.round(uint32_t(i));

        sim_assert(md5.a == refImpl.a) << "a in round " << i;
        sim_assert(md5.b == refImpl.b) << "b in round " << i;
        sim_assert(md5.c == refImpl.c) << "c in round " << i;
        sim_assert(md5.d == refImpl.d) << "d in round " << i;
    }

    BOOST_TEST(refImpl.a + 0x67452301 == 0xd98c1dd4);
    md5.endBlock();

    BVec hash = md5.finalize();
    BVec ref = "xD41D8CD98F00B204E9800998ECF8427E";

    sim_assert(hash(0, 64) == ref(0, 64)) << hash << " != " << ref; // TODO: implement large compare simulation
    sim_assert(hash(64, 64) == ref(64, 64)) << hash << " != " << ref;

    eval(design.getCircuit());
}
