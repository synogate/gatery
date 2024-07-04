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

#include <gatery/scl/io/BitBangEngine.h>

using namespace gtry;
using namespace gtry::scl;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(bitbang_set_get_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.ioClk(-1)
		.ioMosi(-1)
		.ioMiso(-1)
		.generate(move(command), 14);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		fork([&]()->SimProcess {
			while (true)
			{
				simu(ready(result)) = '0';
				for(size_t i = 0; i < 13; ++i)
					co_await OnClk(clock);
				simu(ready(result)) = '1';
				co_await OnClk(clock);
			}
		});

		for (size_t i = 0; i < 14; ++i)
			simu(engine.io(i).in) = (i % 2) ? '1' : '0';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::bad_command,
			(uint8_t)BitBangEngine::Command::set_byte0,	0xF0, 0xFF,
			(uint8_t)BitBangEngine::Command::set_byte1,	0x05, 0x0F,
			(uint8_t)BitBangEngine::Command::get_byte0,
			(uint8_t)BitBangEngine::Command::get_byte1,
		};

		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		std::vector<uint8_t> expected_results = {
			(uint8_t)BitBangEngine::Command::bad_command_response, (uint8_t)BitBangEngine::Command::bad_command,
			0xF0, 0x25,
		};

		for (uint8_t expected : expected_results)
		{
			co_await performTransferWait(result, clock);
			BOOST_TEST(simu(*result) == expected);
		}

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_serialize_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine.generate(move(command), 14);
	engine.pin("io");
	pinOut(result, "result");

	Bit expectedClock = pinIn().setName("expectedClock");
	Bit expectedData = pinIn().setName("expectedData");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';
		simu(expectedData) = 'x';
		simu(expectedClock) = 'x';
//		fork([&]()->SimProcess {
//			while (true)
//			{
//				simu(ready(result)) = '0';
//				for (size_t i = 0; i < 13; ++i)
//					co_await OnClk(clock);
//				simu(ready(result)) = '1';
//				co_await OnClk(clock);
//			}
//		});

		for (size_t combination = 0; combination < 8; ++combination)
		{
			size_t setupEdge = (combination >> 0) & 1;
			size_t captureEdge = (combination >> 1) & 1;
			size_t initialClock = (combination >> 2) & 1;

			std::string commonName = std::to_string(combination) + " CPOL" + std::to_string(initialClock);
			if (setupEdge != captureEdge)
			{
				if (initialClock != setupEdge)
					commonName += " CPHA0";
				else
					commonName += " CPHA1";
			}
			sim::SimulationContext::current()->onDebugMessage(nullptr, commonName);

			std::vector<uint8_t> cmdInitialLineState = {
				(uint8_t)BitBangEngine::Command::set_byte0,	uint8_t(0xFE | initialClock), 0xFF,
			};
			co_await strm::sendPacket(command, strm::SimPacket(cmdInitialLineState), clock);

			std::vector<uint8_t> cmdSerialize = {
				uint8_t(0x30 | setupEdge | (captureEdge << 2)), /* length */ 0x01, 0x00, /* data */ 0x55, 0xAA,
			};
			co_await strm::sendPacket(command, strm::SimPacket(cmdSerialize), clock);

			for(size_t i = 0; i < 2; ++i)
				co_await OnClk(clock);
			//co_await strm::sendBeat(command, 0xA0 | combination, clock);
			//for (size_t i = 0; i < 2; ++i)
			//	co_await OnClk(clock);

			
		}

		stopTest();
		co_return;
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 40, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_clock_only_bytes_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine.generate(move(command), 3);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> cmdCPOL0 = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0xFE, 0x01,
			(uint8_t)BitBangEngine::Command::toggle_clock_bytes, 0x01, 0x00,
		};
		co_await strm::sendPacket(command, strm::SimPacket(cmdCPOL0), clock);

		std::vector<uint8_t> cmdCPOL1 = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0xFF, 0x01,
			(uint8_t)BitBangEngine::Command::toggle_clock_bytes, 0x01, 0x00,
			0xAA
		};
		co_await strm::sendPacket(command, strm::SimPacket(cmdCPOL1), clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_clock_only_bits_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine.generate(move(command), 1);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x01,
			(uint8_t)BitBangEngine::Command::toggle_clock_bits, 0x00,
			(uint8_t)BitBangEngine::Command::toggle_clock_bits, 0x01,
			(uint8_t)BitBangEngine::Command::set_byte0,	0x01, 0x01,
			(uint8_t)BitBangEngine::Command::toggle_clock_bits, 0x00,
			(uint8_t)BitBangEngine::Command::toggle_clock_bits, 0x01,
			0xAA
		};
		co_await strm::sendPacket(command, strm::SimPacket(commands), clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_three_phase_clocking_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.ioMiso(1) // use same io for input and output to loop back data
		.generate(move(command), 2);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x0F,
			(uint8_t)BitBangEngine::Command::threephase_clock_enable,
			0x31, 0x00, 0x00, 0x5A,
		};
		co_await strm::sendPacket(command, strm::SimPacket(commands), clock);

		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x5A);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_loopback_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 3);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x04, 0x0F,
			0x31, 0x00, 0x00, 0x5A,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			0x31, 0x00, 0x00, 0x5A,
			(uint8_t)BitBangEngine::Command::loopback_disable,
			0x31, 0x00, 0x00, 0x5A,
		};
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFF);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x5A);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFF);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_lsb_first_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 3);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x0F,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			0x39, 0x01, 0x00, 0x55, 0xAA,
		};
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x55);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_serialize_bits_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 3);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x0F,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			0x33, 0x0F, 0x55, 0xAA,
			0x33, 0x00, 0x80,
			0x3B, 0x00, 0x01,
			0x3B, 0x0D, 0x5A, 0x05
		};
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x55);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x01);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x80);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x5A);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x14);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_tms_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 4);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x0F,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			0x63, 2*7-1, 0x85, 0x0A,
			0x6B, 2*7-1, 0x85, 0x0A,
			0x63, 0, 0xF0,
		};
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x7F);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x00);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFE);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x00);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x01);

		co_await OnClk(clock);
		co_await OnClk(clock);
		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_stop_clock_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.ioMosi(-1)
		.ioStopClock(1)
		.generate(move(command), 2);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x01,
			0x94,
			0xAA,
		};

		simu(engine.io(1).in) = '0';
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for(size_t i = 0; i < 16; ++i)
			co_await OnClk(clock);

		simu(engine.io(1).in) = '1';
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFA);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);

		commands[3]++; // wait for low
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 12; ++i)
			co_await OnClk(clock);

		simu(engine.io(1).in) = '0';
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFA);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);


		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_stop_clock_timeout_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.ioMosi(-1)
		.ioStopClock(1)
		.generate(move(command), 2);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x01,
			0x9C, 0x01, 0x00,
			0xAA,
		};

		simu(engine.io(1).in) = '0';
		for (size_t i = 0; i < 2; ++i)
		{
			// abort by gpio high
			fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

			for (size_t i = 0; i < 16; ++i)
				co_await OnClk(clock);

			simu(engine.io(1).in) = !simu(engine.io(1).in);
			co_await performTransferWait(result, clock);
			BOOST_TEST(simu(*result) == 0xFA);
			co_await performTransferWait(result, clock);
			BOOST_TEST(simu(*result) == 0xAA);

			// wait for timeout
			simu(engine.io(1).in) = !simu(engine.io(1).in);
			fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

			co_await performTransferWait(result, clock);
			BOOST_TEST(simu(*result) == 0xFA);
			co_await performTransferWait(result, clock);
			BOOST_TEST(simu(*result) == 0xAA);
		
			// inverse wait condition
			commands[3]++;
			simu(engine.io(1).in) = !simu(engine.io(1).in);
		}

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 20, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_stop_signal_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.ioClk(-1)
		.ioStopClock(0)
		.generate(move(command), 1);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			0x88,
			0xAA,
		};

		simu(engine.io(0).in) = '0';
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 16; ++i)
			co_await OnClk(clock);

		simu(engine.io(0).in) = '1';
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFA);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);

		commands[0]++; // wait for low
		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 12; ++i)
			co_await OnClk(clock);

		simu(engine.io(0).in) = '0';
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xFA);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0xAA);


		co_await OnClk(clock);
		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_fast_bangmode_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 6);
	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x00, 0x0F,
			(uint8_t)BitBangEngine::Command::set_clock_div, 0x01, 0x00,
		};
		for (size_t i = 0; i < 32; ++i)
			commands.push_back(uint8_t(0xC0 | i));

		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 64; ++i)
			co_await OnClk(clock);


		co_await OnClk(clock);
		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_open_drain_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 5);

	engine.pin("io");
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x33, 0x0F,
			(uint8_t)BitBangEngine::Command::set_open_drain, 0x06, 0x00,
		};
		co_await strm::sendPacket(command, strm::SimPacket(commands), clock);

		co_await OnClk(clock);
		BOOST_TEST(simu(engine.io(0).in) == '1');
		BOOST_TEST(!simu(engine.io(1).in).defined());
		BOOST_TEST(simu(engine.io(2).in) == '0');
		BOOST_TEST(simu(engine.io(3).in) == '0');
		BOOST_TEST(!simu(engine.io(4).in).defined());


		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_clock_stratching_spi_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 3);

	engine.pin("io", { .highImpedanceValue = PinNodeParameter::HighImpedanceValue::PULL_UP });
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x03, 0x03,
			(uint8_t)BitBangEngine::Command::set_open_drain, 0x03, 0x00,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			(uint8_t)0x33, 0x07, 0x5A,
		};

		fork([&]()->SimProcess {
			while (true)
			{
				simu(engine.io(0).in) = '0';
				for (size_t i = 0; i < 4; ++i)
					co_await OnClk(clock);
				simu(engine.io(0).in) = 'z';
				co_await OnClk(clock);
			}
		});

		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 5 * 8; ++i)
			co_await OnClk(clock);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x5A);

		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(bitbang_clock_stratching_i2c_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 });
	ClockScope clkScp(clock);

	RvStream<BVec> command{ 8_b };
	pinIn(command, "command");

	BitBangEngine engine;
	RvStream<BVec> result = engine
		.generate(move(command), 3);

	engine.pin("io", { .highImpedanceValue = PinNodeParameter::HighImpedanceValue::PULL_UP });
	pinOut(result, "result");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(result)) = '1';

		std::vector<uint8_t> commands = {
			(uint8_t)BitBangEngine::Command::set_byte0,	0x03, 0x03,
			(uint8_t)BitBangEngine::Command::set_open_drain, 0x03, 0x00,
			(uint8_t)BitBangEngine::Command::loopback_enable,
			(uint8_t)BitBangEngine::Command::threephase_clock_enable,
			(uint8_t)0x33, 0x07, 0x5A,
		};

		fork([&]()->SimProcess {
			while (true)
			{
				simu(engine.io(0).in) = '0';
				for (size_t i = 0; i < 4; ++i)
					co_await OnClk(clock);
				simu(engine.io(0).in) = 'z';
				co_await OnClk(clock);
			}
		});

		fork(strm::sendPacket(command, strm::SimPacket(commands), clock));

		for (size_t i = 0; i < 5 * 8; ++i)
			co_await OnClk(clock);
		co_await performTransferWait(result, clock);
		BOOST_TEST(simu(*result) == 0x5A);

		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 10, 1'000'000 }));
}
