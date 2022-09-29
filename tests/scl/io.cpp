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

#include <gatery/simulation/Simulator.h>
#include <gatery/scl/io/codingNRZI.h>
#include <gatery/scl/io/RecoverDataDifferential.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


BOOST_FIXTURE_TEST_CASE(SimProc_Basics, BoostUnitTestSimulationFixture)
{
	using namespace gtry;



	unsigned baudRate = 19200;
	Clock clock({ .absoluteFrequency = baudRate*5 });
	std::vector<std::uint8_t> dataStream;
	{
		ClockScope clkScp(clock);
		auto rx = pinIn().setName("inRx");
		sim_tap((Bit)rx);

		gtry::scl::UART uart;
		uart.baudRate = baudRate;


		auto stream = uart.receive(rx);

		auto outData = pinOut(stream.data).setName("outData");
		auto outValid = pinOut(stream.valid).setName("outValid");
		auto outReady = pinIn().setName("outReady");
		stream.ready = outReady;



		auto sending = pinIn().setName("sending");
		sim_tap((Bit)sending);

		addSimulationProcess([rx, &clock, &dataStream, sending, baudRate]()->SimProcess{
			dataStream.clear();
			simu(rx) = '1';
			simu(sending) = '0';

			co_await WaitFor(Seconds(2)/clock.absoluteFrequency());
			while (true) {
				std::uint8_t data = rand();
				dataStream.push_back(data);


				simu(sending) = '1';
				simu(rx) = '0'; // start bit
				co_await WaitFor(Seconds(1,baudRate));
				for ([[maybe_unused]]  auto i : gtry::utils::Range(8)) {
					simu(rx) = (data & 1) == 1; // data bit
					data >>= 1;
					co_await WaitFor(Seconds(1,baudRate));
				}
				simu(rx) = '1'; // stop bit
				co_await WaitFor(Seconds(1,baudRate));
				simu(sending) = '0';

				co_await WaitFor(Seconds(rand() % 100)/clock.absoluteFrequency());
			}
		});

		addSimulationProcess([=, &clock, &dataStream]()->SimProcess{
			simu(outReady) = false;
			co_await WaitFor(Seconds(1,2)/clock.absoluteFrequency());

			simu(outReady) = true;

			size_t readIdx = 0;
			while (true) {
				while (!simu(outValid))
					co_await WaitClk(clock);

				size_t data = simu(outData);
				BOOST_TEST(readIdx < dataStream.size());
				if (readIdx < dataStream.size())
					BOOST_TEST(data == dataStream[readIdx]);

				readIdx++;

				co_await WaitClk(clock);
			}
		});

	}

	design.postprocess();
	runTicks(clock.getClk(), 500);
}



void setup_recoverDataDifferential(hlim::ClockRational actualBusClockFrequency, size_t chipMultiplier, BoostUnitTestSimulationFixture &fixture)
{
	Clock clock({ 
		.absoluteFrequency = 12'000'000*chipMultiplier,
		.name = "clock",
	});
	ClockScope scp(clock);

	Clock busClock({ 
		.absoluteFrequency = 12'000'000,
		.name = "busClock",
	});
	Clock actualBusClock({ 
		.absoluteFrequency = actualBusClockFrequency,
		.name = "actualBusClock",
	});
	{
		ClockScope scp(actualBusClock);
		Bit dummy;
		dummy = reg(dummy);
		pinOut(dummy);
	}


	Bit ioP, ioN;
	{
		ClockScope scp(busClock);
		ioP = pinIn().setName("D_plus");
		ioN = pinIn().setName("D_minus");
	}

	scl::VStream<gtry::UInt> stream = scl::recoverDataDifferential(busClock, ioP, ioN);

	auto streamValid = scl::valid(stream);
	pinOut(streamValid, "out_valid");
	auto streamData = *stream;
	pinOut(streamData, "out_data");

	std::vector<std::pair<bool, bool>> dataStream;
	std::mt19937 rng(1337);

	// Generation
	fixture.getSimulator().addSimulationProcess([actualBusClock, &dataStream, &rng, ioP, ioN]()->SimProcess{
		std::uniform_real_distribution<float> randomBitDistribution(0.1f, 0.9f);
		std::uniform_real_distribution<float> randomUniform(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomBurstLength(2, 7);

		dataStream.clear();

		// pseudo start bit sequence
		simu(ioP) = '1';
		simu(ioN) = '0';
		co_await WaitClk(actualBusClock);
		co_await WaitClk(actualBusClock);
		co_await WaitClk(actualBusClock);

		simu(ioP) = '0';
		simu(ioN) = '1';
		co_await WaitClk(actualBusClock);

		while (true) {
			float bitDist = randomBitDistribution(rng);
			size_t burstLength = randomBurstLength(rng);
			for ([[maybe_unused]] auto i : gtry::utils::Range(burstLength)) {

				std::pair<bool, bool> beat = {
					randomUniform(rng) > bitDist,
					randomUniform(rng) < bitDist,
				};

				simu(ioP) = beat.first;
				simu(ioN) = beat.second;
				dataStream.push_back(beat);

				co_await WaitClk(actualBusClock);
			}
			// Ensure a clock edge after every at most 6 bits
			auto beat = dataStream.back();
			beat.first = !beat.first;
			beat.second = !beat.second;
			simu(ioP) = beat.first;
			simu(ioN) = beat.second;
			dataStream.push_back(beat);

			co_await WaitClk(actualBusClock);
		}
	});

	// Verification
	size_t numBeatsVerified = 0;
	fixture.getSimulator().addSimulationProcess([&fixture, clock, &dataStream, streamValid, streamData, &numBeatsVerified]()->SimProcess{
		numBeatsVerified = 0;

		auto waitForStreamEqual = [&](size_t value)->SimProcess{
			while (true) {
				BOOST_TEST(simu(streamValid).defined());
				if (simu(streamValid)) {
					BOOST_TEST(simu(streamData).defined());
					if (simu(streamData) == value)
						break;
				}
				co_await WaitClk(clock);
			}
		};
		// wait for pseudo start bit sequence
		co_await waitForStreamEqual(1);
		co_await waitForStreamEqual(2);
		co_await WaitClk(clock);

		while (true) {
			BOOST_TEST(simu(streamValid).defined());
			if (simu(streamValid)) {
				BOOST_TEST(simu(streamData).defined());
				BOOST_REQUIRE(numBeatsVerified < dataStream.size());

				const auto &beat = dataStream[numBeatsVerified];
				size_t beatUInt = (beat.first?1:0) | (beat.second?2:0);
				BOOST_TEST(simu(streamData) == beatUInt);

				numBeatsVerified++;
			}
			co_await WaitClk(clock);
		}
	});

	fixture.getDesign().postprocess();

	fixture.runFixedLengthTest(hlim::ClockRational{1'000, 12'000'000});

	BOOST_TEST(numBeatsVerified > 900);
}

BOOST_FIXTURE_TEST_CASE(recoverDataDifferential_faster_3, BoostUnitTestSimulationFixture)
{
	setup_recoverDataDifferential({12'500'000,1}, 3, *this); // 12.5 MHz bus clock, 3x oversampling
}

BOOST_FIXTURE_TEST_CASE(recoverDataDifferential_slower_3, BoostUnitTestSimulationFixture)
{
	setup_recoverDataDifferential({11'500'000,1}, 3, *this); // 11.5 MHz bus clock, 3x oversampling
}

BOOST_FIXTURE_TEST_CASE(recoverDataDifferential_faster_10, BoostUnitTestSimulationFixture)
{
	setup_recoverDataDifferential({12'500'000,1}, 10, *this); // 12.5 MHz bus clock, 10x oversampling
}

BOOST_FIXTURE_TEST_CASE(recoverDataDifferential_slower_10, BoostUnitTestSimulationFixture)
{
	setup_recoverDataDifferential({11'500'000,1}, 10, *this); // 11.5 MHz bus clock, 10x oversampling
}