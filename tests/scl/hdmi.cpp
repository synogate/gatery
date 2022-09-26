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


using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(tmdsReduction, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	uint32_t random = std::random_device{}();
	auto a = ConstUInt(random & 0xFF, 8_b);

	UInt encoded = gtry::scl::hdmi::tmdsEncodeReduceTransitions(a);
	BOOST_TEST(encoded.width() == a.width() + 1);

	UInt decoded = gtry::scl::hdmi::tmdsDecodeReduceTransitions(encoded);
	sim_assert(a == decoded) << "decode(encoder()) mismatch: input:" << a << " decoded " << decoded;
	sim_debug() << a << " => " << encoded << " => " << decoded << " | " << gtry::scl::bitcount(a);

	eval();
}

BOOST_FIXTURE_TEST_CASE(tmdsBitflip, gtry::BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope scope(clock);
	
	UInt test_counter = 8_b;
	test_counter = reg(test_counter, "8b0");

	UInt encoded = gtry::scl::hdmi::tmdsEncodeBitflip(clock, test_counter);
	BOOST_TEST(test_counter.width() == encoded.width() - 1);

	UInt decoded = gtry::scl::hdmi::tmdsDecodeBitflip(encoded);
	sim_assert(decoded == test_counter);

	test_counter += 1;

	design.postprocess();
	runTicks(clock.getClk(), 260);
}

