#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;
using namespace hcl::core::frontend;

struct SimpleStruct
{
    hcl::core::frontend::BVec vec = hcl::core::frontend::BVec(3_b);
    hcl::core::frontend::Bit bit;
};

BOOST_HANA_ADAPT_STRUCT(SimpleStruct, vec, bit);

struct RichStruct : SimpleStruct
{
    std::vector<SimpleStruct> list;
    int parameter = 5;
};

BOOST_HANA_ADAPT_STRUCT(RichStruct, vec, bit, list, parameter);

BOOST_FIXTURE_TEST_CASE(CompoundName, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Bit bit;
    setName(bit, "bit");
    BOOST_CHECK(bit.getName() == "bit");

    BVec vec{4_b };
    setName(vec, "vec");
    BOOST_CHECK(vec.getName() == "vec");

    std::vector<BVec> vecvec( 3, vec );
    setName(vecvec, "vecvec");
    BOOST_CHECK(vecvec[0].getName() == "vecvec0");
    BOOST_CHECK(vecvec[1].getName() == "vecvec1");
    BOOST_CHECK(vecvec[2].getName() == "vecvec2");

    RichStruct obj;
    obj.list.emplace_back();
    setName(obj, "obj");
    BOOST_CHECK(obj.list[0].vec.getName() == "obj_list0_vec");
}

BOOST_FIXTURE_TEST_CASE(CompoundWidth, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Bit bit;
    BOOST_TEST(width(bit) == 1);

    BVec vec{ 4_b };
    BOOST_TEST(width(vec) == 4);

    std::vector<BVec> vecvec( 3, vec );
    BOOST_TEST(width(vecvec) == 3*4);

}

BOOST_FIXTURE_TEST_CASE(CompoundPack, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    {
        Bit bit = '1';
        BVec bitPack = pack(bit);
        sim_assert(bitPack[0] == '1');
    }

    {
        BVec vec = 5;
        BVec vecPack = pack(vec);
        sim_assert(vecPack == 5);
    }

    {
        BVec vec = 5u;
        std::vector<BVec> vecvec( 3, vec );
        BVec vecPack = pack(vecvec);
        sim_assert(vecPack(0,3) == 5u);
        sim_assert(vecPack(3,3) == 5u);
        sim_assert(vecPack(6,3) == 5u);
    }

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(CompoundUnpack, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    RichStruct in;
    in.vec = 5u;
    in.bit = '0';
    for (size_t i = 0; i < 7; ++i)
    {
        in.list.emplace_back();
        in.list.back().vec = ConstBVec(i, 3);
        in.list.back().bit = i < 4;
    }

    BVec inPacked = pack(in);

    RichStruct out;
    out.list.resize(in.list.size());
    unpack(inPacked, out);

    sim_assert(out.vec == 5u);
    sim_assert(out.bit == '0');
    for (size_t i = 0; i < 7; ++i)
    {
        sim_assert(out.list[i].vec == ConstBVec(i, 3));
        sim_assert(out.list[i].bit == (i < 4));
    }
    
    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(ConstructFromSignal, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Bit sbit = '1';
    Bit dbit = constructFrom(sbit);
    sim_assert(sbit == '1');

    BVec svec = "0x101A";
    BVec dvec = constructFrom(svec);
    sim_assert(svec == "0x101A");

    int sval = 5;
    int dval = constructFrom(sval);
    BOOST_TEST(sval == 5);
    BOOST_TEST(dval == 5);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(ConstructFromCompound, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    std::array<Bit, 4> fixedContainerSrc = { {'1', '0', '1', '1'} };
    std::array<Bit, 4> fixedContainerDst = constructFrom(fixedContainerSrc);
    sim_assert(fixedContainerSrc[0] == '1');

    std::vector<Bit> dynamicContainerSrc = { '1', '0', '1', '1' };
    std::vector<Bit> dynamicContainerDst = constructFrom(dynamicContainerSrc);
    sim_assert(dynamicContainerSrc[0] == '1');

    RichStruct in;
    in.vec = 5u;
    in.bit = '0';
    in.parameter = 13;
    for (size_t i = 0; i < 7; ++i)
    {
        in.list.emplace_back();
        in.list.back().vec = ConstBVec(i, 3);
        in.list.back().bit = i < 4;
    }

    RichStruct out = constructFrom(in);
    BOOST_TEST(in.parameter == out.parameter);

    eval(design.getCircuit());
}

