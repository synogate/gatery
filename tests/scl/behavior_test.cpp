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

#include <gatery/scl/algorithm/BehaviorTree.h>


using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(bt_selector_test, BoostUnitTestSimulationFixture)
{
	std::array<scl::bt::BehaviorStream, 3> down;
	pinOut(down, "down");

	scl::bt::BehaviorStream up = scl::bt::Selector{ "selector", 
		down[0], 
		down[1], 
		down[2] 
	}();
	pinIn(up, "up");

	Clock clock({ .absoluteFrequency = 100'000'000 });
	addSimulationProcess([&]()->SimProcess {

		for (auto& s : down)
		{
			simu(ready(s)) = '0';
			simu(*s->success) = '0';
		}
		simu(valid(up)) = '0';

		co_await AfterClk(clock);

		for (auto& s : down)
			BOOST_TEST(simu(valid(s)) == '0');
		BOOST_TEST(simu(ready(up)) == '0');
		co_await AfterClk(clock);

		
		for (size_t input = 0; input < 1 << 6; ++input)
		{
			size_t state = input;
			for (auto& s : down)
			{
				simu(ready(s)) = (state & 1) == 1; state >>= 1;
				simu(*s->success) = (state & 1) == 1; state >>= 1;
			}
			simu(valid(up)) = '1';
			co_await AfterClk(clock);

			size_t i;
			// check all childs up to the first success or running
			for (i = 0; i < down.size(); ++i)
			{
				BOOST_TEST(simu(valid(down[i])) == '1');

				if (simu(ready(down[i])) == '0')
					break;
				if (simu(*down[i]->success) == '1')
				{
					BOOST_TEST(simu(*up->success) == '1');
					break;
				}
			}
			// all childs failed -> selector fails
			if (i == down.size())
			{
				BOOST_TEST(simu(ready(up)) == '1');
				BOOST_TEST(simu(*up->success) == '0');
			}
			// check activation state of other childs
			for (++i; i < down.size(); ++i)
			{
				BOOST_TEST(simu(valid(down[i])) == '0');
			}

			simu(valid(up)) = '0';
			co_await AfterClk(clock);
			for (auto& s : down)
				BOOST_TEST(simu(valid(s)) == '0');
		}
		stopTest();
	});

	design.postprocess();
	//dbg::vis();

	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(bt_sequence_test, BoostUnitTestSimulationFixture)
{
	std::array<scl::bt::BehaviorStream, 3> down;
	pinOut(down, "down");

	scl::bt::BehaviorStream up = scl::bt::Sequence{ "sequence",
		down[0],
		down[1],
		down[2]
	}();
	pinIn(up, "up");

	Clock clock({ .absoluteFrequency = 100'000'000 });
	addSimulationProcess([&]()->SimProcess {

		for (auto& s : down)
		{
			simu(ready(s)) = '0';
			simu(*s->success) = '0';
		}
		simu(valid(up)) = '0';

		co_await AfterClk(clock);

		for (auto& s : down)
			BOOST_TEST(simu(valid(s)) == '0');
		BOOST_TEST(simu(ready(up)) == '0');
		co_await AfterClk(clock);

		simu(valid(up)) = '1';
		for (size_t input = 0; input < 1 << 6; ++input)
		{
			size_t state = input;
			for (auto& s : down)
			{
				simu(ready(s)) = (state & 1) == 1; state >>= 1;
				simu(*s->success) = (state & 1) == 1; state >>= 1;
			}
			co_await AfterClk(clock);

			size_t i;
			// check all childs up to the first fail or running
			for (i = 0; i < down.size(); ++i)
			{
				BOOST_TEST(simu(valid(down[i])) == '1');

				if (simu(ready(down[i])) == '0')
					break;
				if (simu(*down[i]->success) == '0')
				{
					BOOST_TEST(simu(*up->success) == '0');
					break;
				}
			}
			// all childs succeeded -> sequence success
			if (i == down.size())
			{
				BOOST_TEST(simu(ready(up)) == '1');
				BOOST_TEST(simu(*up->success) == '1');
			}
			// check activation state of other childs
			for (++i; i < down.size(); ++i)
			{
				BOOST_TEST(simu(valid(down[i])) == '0');
			}
		}
		stopTest();
	});

	design.postprocess();
	//dbg::vis();

	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(bt_check_test, BoostUnitTestSimulationFixture)
{
	Bit condition = pinIn().setName("condition");

	scl::bt::BehaviorStream up = scl::bt::Check{ condition }();
	pinIn(up, "up");

	Clock clock({ .absoluteFrequency = 100'000'000 });
	addSimulationProcess([&]()->SimProcess {

		simu(condition) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '1');
		BOOST_TEST(simu(*up->success) == '0');

		simu(condition) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '1');
		BOOST_TEST(simu(*up->success) == '1');
		stopTest();
	});

	design.postprocess();
	//dbg::vis();

	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(bt_wait_test, BoostUnitTestSimulationFixture)
{
	Bit condition = pinIn().setName("condition");

	scl::bt::BehaviorStream up = scl::bt::Wait{condition}();
	pinIn(up, "up");

	Clock clock({ .absoluteFrequency = 100'000'000 });
	addSimulationProcess([&]()->SimProcess {

		simu(condition) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '0');

		simu(condition) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '1');
		BOOST_TEST(simu(*up->success) == '1');
		stopTest();
	});

	design.postprocess();
	//dbg::vis();

	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(bt_do_test, BoostUnitTestSimulationFixture)
{
	Bit status = pinIn().setName("status");

	scl::bt::BehaviorStream up = scl::bt::Do{ [&]() {
		return status;
	} }();
	pinIn(up, "up");

	Clock clock({ .absoluteFrequency = 100'000'000 });
	addSimulationProcess([&]()->SimProcess {

		simu(valid(up)) = '1';
		simu(status) = '0';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '1');
		BOOST_TEST(simu(*up->success) == '0');

		simu(status) = '1';
		co_await AfterClk(clock);
		BOOST_TEST(simu(ready(up)) == '1');
		BOOST_TEST(simu(*up->success) == '1');
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 128);
}
