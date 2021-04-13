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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::sim::UnitTestSimulationFixture, tmdsReduction, data::xrange(255), val)
{
    using namespace hcl;

    DesignScope design;

    auto a = ConstBVec(val, 8);

    BVec encoded = hcl::scl::hdmi::tmdsEncodeReduceTransitions(a);
    BOOST_TEST(encoded.getWidth() == a.getWidth() + 1);

    BVec decoded = hcl::scl::hdmi::tmdsDecodeReduceTransitions(encoded);
    sim_assert(a == decoded) << "decode(encoder()) mismatch: input:" << a << " decoded " << decoded;
    sim_debug() << a << " => " << encoded << " => " << decoded << " | " << hcl::scl::bitcount(a);

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(tmdsBitflip, hcl::sim::UnitTestSimulationFixture)
{
    using namespace hcl;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);
    
    Register<BVec> test_counter(8_b);
    test_counter.setReset("8b0");
    test_counter += 1;

    BVec encoded = hcl::scl::hdmi::tmdsEncodeBitflip(clock, test_counter.delay(1));
    BOOST_TEST(test_counter.getWidth() == encoded.getWidth() - 1);

    BVec decoded = hcl::scl::hdmi::tmdsDecodeBitflip(encoded);
    sim_assert(decoded == test_counter.delay(1));

    design.getCircuit().optimize(3);
    runTicks(design.getCircuit(), clock.getClk(), 260);
}

