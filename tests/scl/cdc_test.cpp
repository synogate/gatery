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

#include <gatery/scl/cdc.h>
#include <gatery/scl/stream/Stream.h>

using namespace gtry;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(grayCode, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	UInt a = pinIn(4_b).setName("a");
	BVec b = scl::grayEncode(a);
	pinOut(b).setName("gray");
	UInt c = scl::grayDecode(b);
	pinOut(c).setName("decoded");

	addSimulationProcess([&]()->SimProcess {
		for(size_t i = 0; i < 16; ++i)
		{
			simu(a) = i;
			co_await WaitStable();
			BOOST_TEST(simu(b) == (i ^ (i >> 1)));
			BOOST_TEST(simu(c) == i);
			co_await AfterClk(clock);
		}

		stopTest();
	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "graycode.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 2048);
}

