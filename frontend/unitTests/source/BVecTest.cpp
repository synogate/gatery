#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(BVecIterator, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = "b1100";
    BOOST_TEST(a.size() == 4);
    BOOST_TEST(!a.empty());

    size_t counter = 0;
    for (auto it = a.cbegin(); it != a.cend(); ++it)
    {
        if (counter < 2)
            sim_assert(!*it);
        else
            sim_assert(*it);

        counter++;

    }
    BOOST_TEST(a.size() == counter);

    counter = 0;
    for (auto &b : a)
        counter++;
    BOOST_TEST(a.size() == counter);

    sim_assert(a[0] == false) << "a[0] is " << a[0] << " but should be false";
    sim_assert(a[1] == false) << "a[1] is " << a[1] << " but should be false";
    sim_assert(a[2] == true) << "a[2] is " << a[2] << " but should be true";
    sim_assert(a[3] == true) << "a[3] is " << a[3] << " but should be true";

    a[0] = true;
    sim_assert(a[0] == true) << "a[0] is " << a[0] << " after setting it explicitely to true";

    for (auto &b : a)
        b = true;
    sim_assert(a[1] == true) << "a[1] is " << a[1] << " after setting all bits to true";


    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(BVecIteratorArithmetic, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = "b1100";

    auto it1 = a.begin();
    auto it2 = it1 + 1;
    BOOST_CHECK(it1 != it2);
    BOOST_CHECK(it1 <= it2);
    BOOST_CHECK(it1 < it2);
    BOOST_CHECK(it2 >= it1);
    BOOST_CHECK(it2 > it1);
    BOOST_CHECK(it1 == a.begin());
    BOOST_CHECK(it2 - it1 == 1);
    BOOST_CHECK(it2 - 1 == it1);

    auto it3 = it1++;
    BOOST_CHECK(it3 == a.begin());
    BOOST_CHECK(it1 == it2);

    auto it4 = it1--;
    BOOST_CHECK(it4 == it2);
    BOOST_CHECK(it1 == a.begin());

    auto it5 = ++it1;
    BOOST_CHECK(it5 == it1);
    BOOST_CHECK(it5 == it2);

    it5 = --it1;
    BOOST_CHECK(it5 == it1);
    BOOST_CHECK(it5 == a.begin());
}

BOOST_FIXTURE_TEST_CASE(BVecFrontBack, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = "b1100";
    sim_assert(!a.front());
    sim_assert(a.back());
    sim_assert(!a.lsb());
    sim_assert(a.msb());

    a.front() = true;
    sim_assert(a.front());

    a.back() = false;
    sim_assert(!a.back());

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(ConstantDataStringParser, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    BOOST_CHECK(parseBVec("32x1bBXx").size() == 32);
    BOOST_CHECK(parseBVec("x1bBX").size() == 16);
    BOOST_CHECK(parseBVec("o170X").size() == 12);
    BOOST_CHECK(parseBVec("b10xX").size() == 4);
}

BOOST_FIXTURE_TEST_CASE(BVecSelectorAccess, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = "b11001110";

    sim_assert(a(2, 4) == "b0011");

    sim_assert(a(1, -1) == "b1100111");
    sim_assert(a(-2, 2) == "b11");

    sim_assert(a(0, 4, 2) == "b1010");
    sim_assert(a(1, 4, 2) == "b1011");

    sim_assert(a(0, 4, 2)(0, 2, 2) == "b00");
    sim_assert(a(0, 4, 2)(1, 2, 2) == "b11");

    eval(design.getCircuit());
}
