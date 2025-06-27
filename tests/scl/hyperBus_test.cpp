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

#include <gatery/scl/memory/HyperRAM.h>

using namespace boost::unit_test;
using namespace gtry;

class HyperBusMasterTest : public BoostUnitTestSimulationFixture
{
public:

	void driveIdle()
	{
		simu(phy.cs) = 0;
		simu(phy.cken) = '0';
		simu(phy.reset) = '0';
		simu(phy.rwds.f).stopDriving();
		simu(phy.dq.f).stopDriving();
	}

protected:
	scl::HyperBusPhy phy = { .cs = 1_b };
};

BOOST_FIXTURE_TEST_CASE(HyperRAMreadTest, HyperBusMasterTest)
{
	Clock clock({ .absoluteFrequency = 100_mhz, .triggerEvent = ClockConfig::TriggerEvent::RISING_AND_FALLING });
	ClockScope clkScp(clock);

	pinIn(phy, "phy");
	scl::hyperRAMSimulationModel(phy, { .latencyDefault = 3 });

	addSimulationProcess([&, this]()->SimProcess {
		driveIdle();
		//co_await OnClk(clock);
		co_await OnClk(clock);

		{
			simu(phy.cs) = '1';
			simu(phy.cken) = '1';

			uint64_t command = 0ull << 47 | 1ull << 45 | 0 << 16 | 0;
			for (size_t i = 0; i < 6; ++i)
			{
				simu(phy.dq.f) = (command >> (5 - i) * 8) & 0xFF;
				co_await OnClk(clock);
			}
			simu(phy.dq.f).stopDriving();

			for (size_t i = 0; i < 4 + 6; ++i)
				co_await OnClk(clock);

			for (size_t i = 0; i < 8; ++i)
			{
				simu(phy.rwds.f) = '1';
				simu(phy.dq.f) = i;
				co_await OnClk(clock);
			}

			driveIdle();
			co_await OnClk(clock);
			co_await OnClk(clock);
		}

		{
			simu(phy.cs) = '1';
			simu(phy.cken) = '1';

			uint64_t command = 1ull << 47 | 1ull << 45 | 0 << 16 | 0;
			for (size_t i = 0; i < 6; ++i)
			{
				simu(phy.dq.f) = (command >> (5 - i) * 8) & 0xFF;
				co_await OnClk(clock);
			}
			simu(phy.dq.f).stopDriving();

			// initial latency + additional latency + 1 + tCKD
			for(size_t i = 0; i < 4 + 6 + 2; ++i)
				co_await OnClk(clock);

			for (size_t i = 0; i < 8; ++i)
			{
				BOOST_TEST(simu(phy.rwds.f) == (i % 2 == 0));
				BOOST_TEST(simu(phy.dq.f) == i);
				co_await OnClk(clock);
			}

			simu(phy.cs) = '0';
			simu(phy.cken) = '0';
			co_await OnClk(clock);
			co_await OnClk(clock);
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout(1us));
}

