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

#include "MappingTests_IO.h"

#include <gatery/scl/io/ddr.h>

#include <boost/test/unit_test.hpp>


using namespace gtry;

void Test_ODDR::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit d1 = pinIn().setName("d1");
	Bit d2 = pinIn().setName("d2");

	Bit o = scl::ddr(d1, d2);
	
	pinOut(o).setName("ddr_output");

	addSimulationProcess([&]()->SimProcess {
		std::mt19937 rng;
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			bool b1 = rng() & 1;
			bool b2 = rng() & 1;
			simu(d1) = b1;
			simu(d2) = b2;

			co_await OnClk(clock);

			co_await WaitFor(Seconds{1,4} / clock.absoluteFrequency());

			BOOST_TEST(simu(o) == b1);

			co_await WaitFor(Seconds{1,2} / clock.absoluteFrequency());

			BOOST_TEST(simu(o) == b2);
		}
		stopTest();
	});

	runTest(Seconds{100,1} / clock.absoluteFrequency());
}


void Test_ODDR_ForClock::execute()
{
	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	Bit o = scl::ddr('1', '0');
	
	pinOut(o).setName("ddr_output");

	addSimulationProcess([&]()->SimProcess {
		for ([[maybe_unused]] auto i : gtry::utils::Range(50)) {
			co_await OnClk(clock);

			co_await WaitFor(Seconds{1,4} / clock.absoluteFrequency());

			BOOST_TEST(simu(o) == '1');

			co_await WaitFor(Seconds{1,2} / clock.absoluteFrequency());

			BOOST_TEST(simu(o) == '0');
		}
		stopTest();
	});

	runTest(Seconds{100,1} / clock.absoluteFrequency());
}