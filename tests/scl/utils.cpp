/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

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

#include <gatery/frontend.h>
#include <gatery/utils.h>
#include <gatery/simulation/UnitTestSimulationFixture.h>

#include <gatery/hlim/supportNodes/Node_SignalGenerator.h>


using namespace gtry;
using namespace gtry::scl;
using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(gtry::sim::UnitTestSimulationFixture, BitCountTest, data::xrange(255) * data::xrange(1, 8), val, bitsize)
{
    DesignScope design;

    BVec a = ConstBVec(val, BitWidth{ uint64_t(bitsize) });
    BVec count = gtry::scl::bitcount(a);
    
    unsigned actualBitCount = gtry::utils::popcount(unsigned(val) & (0xFF >> (8-bitsize)));
    
    BOOST_REQUIRE(count.size() >= (size_t)gtry::utils::Log2(bitsize)+1);
    //sim_debug() << "The bitcount of " << a << " should be " << actualBitCount << " and is " << count;
    sim_assert(count == ConstBVec(actualBitCount, count.getWidth())) << "The bitcount of " << a << " should be " << actualBitCount << " but is " << count;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(gtry::sim::UnitTestSimulationFixture, Decoder, data::xrange(3), val)
{
    DesignScope design;

    OneHot result = decoder(ConstBVec(val, 2_b));
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

BOOST_DATA_TEST_CASE_F(gtry::sim::UnitTestSimulationFixture, ListEncoder, data::xrange(3), val)
{
    DesignScope design;

    OneHot result = decoder(ConstBVec(val, 2_b));
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


BOOST_DATA_TEST_CASE_F(gtry::sim::UnitTestSimulationFixture, PriorityEncoderTreeTest, data::xrange(65), val)
{
    DesignScope design;

    uint64_t testVector = 1ull << val;
    if (val == 54) testVector |= 7;
    if (val == 64) testVector = 0;

    auto res = priorityEncoderTree(ConstBVec(testVector, 64_b), false);
    
    if (testVector)
    {
        BVec ref = gtry::utils::Log2(testVector & -testVector);
        sim_assert(res.valid & res.index == ref) << "wrong index: " << res.index << " should be " << ref;
    }
    else
    {
        sim_assert(!res.valid) << "wrong valid: " << res.valid;
    }

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(addWithCarry, UnitTestSimulationFixture)
{
    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000).setName("clock"));
    ClockScope clkScp(clock);

    BVec a = pinIn(4_b).setName("a");
    BVec b = pinIn(4_b).setName("b");
    Bit cin = pinIn().setName("cin");

    auto [sum, carry] = add(a, b, cin);
    pinOut(sum).setName("sum");
    pinOut(carry).setName("carry");

    addSimulationProcess([&]()->SimProcess {

        for (size_t carryMode = 0; carryMode < 2; ++carryMode)
        {
            simu(cin) = carryMode;

            for (size_t i = 0; i < a.getWidth().count(); ++i)
            {
                simu(a) = i;
            
                for (size_t j = 0; j < b.getWidth().count(); ++j)
                {
                    simu(b) = j;
                    co_await WaitClk(clock);

                    size_t expectedSum = (i + j + carryMode) & sum.getWidth().mask();
                    BOOST_TEST(simu(sum) == expectedSum);

                    size_t expectedCarry = 0;
                    for (size_t k = 0; k < carry.getWidth().value; ++k)
                    {
                        size_t mask = (1ull << k + 1) - 1;
                        size_t subsum = (i & mask) + (j & mask) + carryMode;
                        expectedCarry |= (subsum & ~mask) >> 1;
                    }
                    BOOST_TEST(simu(carry) == expectedCarry);
                }
            }
        }
    });

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTicks(clock.getClk(), 2048);
}
