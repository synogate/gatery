/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

using namespace boost::unit_test;
using namespace hcl;

using UnitTestSimulationFixture = hcl::UnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(CTS_TestBasics_arith, UnitTestSimulationFixture)
{
    using namespace hcl;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(simu(a).defined() == 0);
    BOOST_TEST(simu(b).defined() == 0);

    BVec c = a + b;

    BOOST_TEST(simu(c).defined() == 0);

    simu(a) = 5;
    BOOST_TEST(simu(c).defined() == 0);

    simu(b) = 10;

    BOOST_TEST(simu(c).defined() == 255);
    BOOST_TEST(simu(c).value() == 15);

    c += 42;

    BOOST_TEST(simu(c).defined() == 255);
    BOOST_TEST(simu(c).value() == 57);
}


BOOST_FIXTURE_TEST_CASE(CTS_TestBasics_logic, UnitTestSimulationFixture)
{
    using namespace hcl;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(simu(a).defined() == 0);
    BOOST_TEST(simu(b).defined() == 0);

    BVec c = a & b;

    BOOST_TEST(simu(c).defined() == 0);

    simu(a) = 7;
    BOOST_TEST(simu(c).defined() == 0);

    simu(b) = 10;

    BOOST_TEST(simu(c).defined() == 255);
    BOOST_TEST(simu(c).value() == (10 & 7));

    c |= 42;

    BOOST_TEST(simu(c).defined() == 255);
    BOOST_TEST(simu(c).value() == (10 & 7 | 42));
}


BOOST_FIXTURE_TEST_CASE(CTS_TestRegisterReset, UnitTestSimulationFixture)
{
    using namespace hcl;


    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    BVec a(8_b);
    a = reg(a, 42);
    BVec b(8_b);
    b = reg(b);

    BOOST_TEST(simu(a).defined() == 255);
    BOOST_TEST(simu(b).defined() == 0);

    BVec c = a + b;

    BOOST_TEST(simu(c).defined() == 0);

    simu(b) = 10;

    BOOST_TEST(simu(c).defined() == 255);
    BOOST_TEST(simu(c).value() == 52);
}
