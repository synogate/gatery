#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

using namespace boost::unit_test;
using namespace hcl::core::frontend;

using UnitTestSimulationFixture = hcl::core::frontend::UnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(CTS_TestBasics_arith, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(sim(a).defined() == 0);
    BOOST_TEST(sim(b).defined() == 0);

    BVec c = a + b;

    BOOST_TEST(sim(c).defined() == 0);

    sim(a) = 5;
    BOOST_TEST(sim(c).defined() == 0);

    sim(b) = 10;

    BOOST_TEST(sim(c).defined() == 255);
    BOOST_TEST(sim(c).value() == 15);

    c += 42;

    BOOST_TEST(sim(c).defined() == 255);
    BOOST_TEST(sim(c).value() == 57);
}


BOOST_FIXTURE_TEST_CASE(CTS_TestBasics_logic, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(sim(a).defined() == 0);
    BOOST_TEST(sim(b).defined() == 0);

    BVec c = a & b;

    BOOST_TEST(sim(c).defined() == 0);

    sim(a) = 7;
    BOOST_TEST(sim(c).defined() == 0);

    sim(b) = 10;

    BOOST_TEST(sim(c).defined() == 255);
    BOOST_TEST(sim(c).value() == (10 & 7));

    c |= 42;

    BOOST_TEST(sim(c).defined() == 255);
    BOOST_TEST(sim(c).value() == (10 & 7 | 42));
}


BOOST_FIXTURE_TEST_CASE(CTS_TestRegisterReset, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a, 42);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(sim(a).defined() == 255);
    BOOST_TEST(sim(b).defined() == 0);

    BVec c = a + b;

    BOOST_TEST(sim(c).defined() == 0);

    sim(b) = 10;

    BOOST_TEST(sim(c).defined() == 255);
    BOOST_TEST(sim(c).value() == 52);
}