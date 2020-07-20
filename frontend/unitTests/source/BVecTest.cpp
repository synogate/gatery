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

    BVec a = 0b1100_bvec;
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
    for (auto& b : a)
        counter++;
    BOOST_TEST(a.size() == counter);

    sim_assert(a[0] == false);
    sim_assert(a[1] == false);
    sim_assert(a[2] == true);
    sim_assert(a[3] == true);

    a[0] = true;
    sim_assert(a[0] == true);

    for (auto& b : a)
        b = true;
    sim_assert(a[1] == true);


    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(BVecIteratorArithmetic, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = 0b1100_bvec;

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

    BVec a = 0b1100_bvec;
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

BOOST_FIXTURE_TEST_CASE(BVecConstFrontBack, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    const BVec a = 0b1100_bvec;
    a.front() = true;
    a.back() = false;

    sim_assert(!a.front());
    sim_assert(a.back());
    sim_assert(!a.lsb());
    sim_assert(a.msb());

    eval(design.getCircuit());
}
