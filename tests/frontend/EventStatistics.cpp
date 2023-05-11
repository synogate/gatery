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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <gatery/frontend/EventStatistics.h>


using namespace boost::unit_test;

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(Statistic_counter, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;


	Clock clock({ .absoluteFrequency = 100'000'000 });

	ClockScope clkScp(clock);


	// Build a simple circuit, and hook up event counters

	UInt value = 4_b;
	Bit watch = value[0];
	
	value = reg(value >> 1, "b1010");
	
	registerEvent("watch_bit", watch);

	
	DesignScope::get()->getCircuit().addSimulationProcess([=]()->SimProcess {
		for(auto i = 0; i <10;i++)
		{
			co_await OnClk(clock);
		}
		stopTest();
		});
	


	// Postprocess and run

	design.postprocess();

	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());

	//EventStatistics::dumpStatistics();  // test full path output

	// �Check the contents of the statistics counters via

	BOOST_TEST(EventStatistics::readEventCounter("top/watch_bit") == 2);
}
