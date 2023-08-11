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
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

using namespace boost::unit_test;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(TestGCD, BoostUnitTestSimulationFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = 10'000, .resetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	auto gcd_ref = [](size_t a, size_t b) {
		while (a != b) {
			if (a > b)
				a -= b;
			else
				b -= a;
		}
		return a;
	};

	size_t x = std::random_device{}() % 254 + 1;
	size_t y = std::random_device{}() % 254 + 1;

	unsigned maxTicks = 200;

	{
		UInt x_vec = ConstUInt(x, 8_b);
		UInt y_vec = ConstUInt(y, 8_b);

		Bit start;
		simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
			context.set(0, context.getTick() == 0);
		}, start);


		UInt result;
		Bit done;

		{
			HCL_NAMED(x_vec);
			HCL_NAMED(y_vec);

			GroupScope entity(GroupScope::GroupType::ENTITY, "gcd");
			entity
				.setComment("Statemachine to compute the GCD of two 8-bit integers.");


			fsm::ImmediateState idle;
			HCL_NAMED(idle);
			fsm::DelayedState running;
			HCL_NAMED(running);

			UInt a = 8_b;
			a = reg(a, "b00000000");
			result = a;

			UInt b = 8_b;
			b = reg(b, "b00000000");
#if 0
			// Euclid's gcd
			idle.onActive([&]{
				IF (start) {
					a = x_vec;
					b = y_vec;
					fsm::delayedSwitch(running);
				}
			});
			running.onActive([&]{
				IF (a == b) {
					fsm::immediateSwitch(idle);
				} ELSE {
					IF (a > b) {
						a = a - b;
					} ELSE {
						b = b - a;
					}
				}
			});
#else
			// Binary GCD
			fsm::ImmediateState shifting;
			HCL_NAMED(shifting);

			UInt d = 4_b;
			d = reg(d, "b0000");

			idle.onActive([&]{
				IF (start) {
					a = x_vec;
					b = y_vec;
					d = ConstUInt(0, 4_b);
					fsm::delayedSwitch(running);
				}
			});
			running.onActive([&]{
				IF (a == b) {
					fsm::immediateSwitch(shifting);
				} ELSE {
					Bit a_odd = a[0];
					Bit b_odd = b[0];
					IF (!a_odd & !b_odd) {
						a >>= 1;
						b >>= 1;
						d += 1;
					}
					IF (!a_odd & b_odd) {
						a >>= 1;
					}
					IF (a_odd & !b_odd) {
						b >>= 1;
					}
					IF (a_odd & b_odd) {
						IF (a < b) {
							UInt help = a;
							a = b;
							b = help;
						} ELSE {
							UInt difference = a - b;
							a = difference >> 1;
						}
					}
				}
			});
			shifting.onActive([&]{
				IF(d == ConstUInt(0, 4_b)) {
					fsm::immediateSwitch(idle);
				} ELSE {
					a <<= 1;
					d -= 1;
				}
			});
#endif
			fsm::FSM stateMachine(clock, idle);
			sim_debug() << result << "," << a;
			HCL_NAMED(result);
			done = stateMachine.isInState(idle);
			HCL_NAMED(done);
		}

		UInt ticks(8_b);
		simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
			context.set(0, context.getTick());
		}, ticks);

		sim_assert((ticks < ConstUInt(maxTicks-1, 8_b)) | done) << "The state machine should be idle after " << maxTicks << " cycles";
		UInt gtruth = ConstUInt(gcd_ref(x, y), 8_b);
		sim_assert((ticks < ConstUInt(maxTicks-1, 8_b)) | (result == gtruth)) << "The state machine computed " << result << " but the correct answer is " << gtruth;
	}

	design.postprocess();
	runTicks(clock.getClk(), maxTicks);
}
