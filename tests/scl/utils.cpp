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

#include <gatery/hlim/supportNodes/Node_SignalGenerator.h>


using namespace gtry;
using namespace gtry::scl;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(BitCountTest, gtry::BoostUnitTestSimulationFixture)
{
	uint32_t random = std::random_device{}();
	UInt a = random;
	UInt count = gtry::scl::bitcount(a);
	
	int actualBitCount = gtry::utils::popcount(random);
	
	sim_assert(count == actualBitCount) << "The bitcount of " << a << " should be " << actualBitCount << " but is " << count;
	eval();
}


BOOST_DATA_TEST_CASE_F(gtry::BoostUnitTestSimulationFixture, Decoder, data::xrange(3), val)
{
	OneHot result = decoder(ConstUInt(val, 2_b));
	BOOST_CHECK(result.size() == 4);
	sim_assert(result == (1u << val)) << "decoded to " << result;

	UInt back = encoder(result);
	BOOST_CHECK(back.size() == 2);
	sim_assert(back == val) << "encoded to " << back;

	scl::VStream<UInt> prio = priorityEncoder(result);
	BOOST_CHECK(prio->size() == 2);
	sim_assert(valid(prio));
	sim_assert(*prio == val) << "encoded to " << *prio;

	eval();
}

BOOST_FIXTURE_TEST_CASE(PriorityEncoderTreeTest, gtry::BoostUnitTestSimulationFixture)
{
	uint32_t testVector = std::random_device{}();

	auto res = priorityEncoderTree(ConstUInt(testVector, 32_b), false);
	
	if (testVector)
	{
		UInt ref = gtry::utils::Log2(gtry::utils::lowestSetBitMask(testVector));
		sim_assert(valid(res) & *res == ref) << "wrong index: " << *res << " should be " << ref;
	}
	else
	{
		sim_assert(!valid(res)) << "wrong valid: " << valid(res);
	}

	eval();
}

BOOST_FIXTURE_TEST_CASE(addWithCarry, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt a = pinIn(4_b).setName("a");
	UInt b = pinIn(4_b).setName("b");
	Bit cin = pinIn().setName("cin");

#ifdef __clang__
	UInt sum, carry;
	std::tie(sum, carry) = add(a, b, cin);
#else
	auto [sum, carry] = add(a, b, cin);
#endif	
	pinOut(sum).setName("sum");
	pinOut(carry).setName("carry");

	addSimulationProcess([&]()->SimProcess {

		for (size_t carryMode = 0; carryMode < 2; ++carryMode)
		{
			simu(cin) = (bool) carryMode;

			for (size_t i = 0; i < a.width().count(); ++i)
			{
				simu(a) = i;
			
				for (size_t j = 0; j < b.width().count(); ++j)
				{
					simu(b) = j;
					co_await AfterClk(clock);

					size_t expectedSum = (i + j + carryMode) & sum.width().mask();
					BOOST_TEST(simu(sum) == expectedSum);

					size_t expectedCarry = 0;
					for (size_t k = 0; k < carry.width().value; ++k)
					{
						size_t mask = gtry::utils::bitMaskRange(0, k + 1);
						size_t subsum = (i & mask) + (j & mask) + carryMode;
						expectedCarry |= (subsum & ~mask) >> 1;
					}
					BOOST_TEST(simu(carry) == expectedCarry);
				}
			}
		}
	});

	design.postprocess();
	runTicks(clock.getClk(), 2048);
}


BOOST_FIXTURE_TEST_CASE(thermometric_test, BoostUnitTestSimulationFixture)
{
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	UInt in = 4_b;
	pinIn(in, "");

	BVec out = uintToThermometric(in);
	pinOut(out, "");

	BOOST_TEST(out.width() == 15_b);
	
	addSimulationProcess([&, this]() -> SimProcess {
		size_t answer = 0;
		for (size_t i = 0; i <= out.size(); i++)
		{
			simu(in) = i;
			co_await WaitStable();
			BOOST_TEST(simu(out) == answer);
			answer <<= 1;
			answer |= 1;
			co_await OnClk(clk);
		}
		stopTest();
	});
	if (false) {
		recordVCD("dut.vcd");
	}
	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 2, 1'000'000 }));
}
