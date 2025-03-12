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
#include <future>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/frontend.h>
#include <gatery/utils.h>

#include <gatery/scl/cdc.h>
#include <gatery/scl/arch/colognechip/SpecialFunction.h>

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

BOOST_FIXTURE_TEST_CASE(eventSyncTest, BoostUnitTestSimulationFixture)
{
		const size_t eventsToSend = 10;

		for (size_t clockIncrement = 0; clockIncrement < 500'000'000; clockIncrement += 16'180'398) {
			Clock inClk({ .absoluteFrequency = 100'000'000 });
			Clock outClk({ .absoluteFrequency = 10'000'000 + clockIncrement});
			ClockScope clkScp(inClk);
			Bit eventIn;
			pinIn(eventIn, "eventIn_" + std::to_string(clockIncrement));

			Bit eventOut = gtry::scl::synchronizeEvent(eventIn, inClk, outClk);
			{
				ClockScope clock(outClk);
				pinOut(eventOut, "eventOut_" + std::to_string(clockIncrement));
			}

			std::shared_ptr<size_t> eventsSent = std::make_shared<size_t>(0);
			std::shared_ptr<size_t> eventsCaught = std::make_shared<size_t>(0);
			
			addSimulationProcess([=]()->SimProcess {
				simu(eventIn) = '0';
				*eventsSent = 0;

				while (*eventsSent < eventsToSend) {
					simu(eventIn) = '1';
					co_await OnClk(inClk);
					simu(eventIn) = '0';
					(*eventsSent)++;
					co_await OnClk(inClk);
					while (*eventsCaught < *eventsSent) {
						co_await OnClk(inClk);
					}
				}
				});

			addSimulationProcess([=, this]()->SimProcess {
				*eventsCaught = 0;
				while (*eventsCaught < eventsToSend) {
					while (simu(eventOut) != '1')
						co_await OnClk(outClk);
					BOOST_TEST(simu(eventOut) == '1');
					co_await OnClk(outClk);
					BOOST_TEST(simu(eventOut) == '0');
					(*eventsCaught)++;
				}
				BOOST_TEST(*eventsSent == *eventsCaught);
				stopTest();
			});
		}

		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(cdc_gatemate_fifo_hw_test, BoostUnitTestSimulationFixture, *boost::unit_test::disabled())
{
	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12'000'000,
		.name = "CLK12M",
		.resetType = hlim::RegisterAttributes::ResetType::NONE
	} };


	ClockScope pinClk{ clk12 };
	Clock clkFast = scl::arch::colognechip::pll({ .absoluteFrequency = {30'000'000}, .resetType = ClockConfig::ResetType::NONE });
	Clock clkSlow = scl::arch::colognechip::pll({ .absoluteFrequency = {29'500'000}, .resetType = ClockConfig::ResetType::NONE });


	scl::Fifo<UInt> toLoop(256, 8_b);
	{
		ClockScope scope(clkSlow);
		Area sender{ "test_generator", true };

		scl::Counter wait{ 10 };
		IF(!wait.isLast())
			wait.inc();

		Reg<UInt> value{ "8x00" };
		IF(!toLoop.full() & wait.isLast())
		{
			toLoop.push(value);
			value = value.current() + 7;
			wait.reset();
		}
	}

	scl::Fifo<UInt> fromLoop(256, 8_b);
	{
		ClockScope scope(clkFast);
		Area receiver{ "test_feedback", true };
		IF(!toLoop.empty() & !fromLoop.full())
		{
			fromLoop.push(toLoop.peek());
			toLoop.pop();
		}
	}

	Bit error;
	{
		ClockScope scope(clkSlow);
		Area checker{ "test_checker", true };

		Reg<UInt> value{ "8x00" };
		IF(!fromLoop.empty())
		{
			error = fromLoop.peek() != value;
			fromLoop.pop();
			value = value.current() + 7;
		}
		error = reg(error, '0');
	}

	toLoop.generate();
	fromLoop.generate();

	ClockScope scope(clkSlow);
	pinOut(error, "UART0_TX");
	
	Bit errorLatch;
	errorLatch = reg(errorLatch | error, '0') | name("errorLatch");
	pinOut(errorLatch, "UART0_RX");

	addSimulationProcess([&]()->SimProcess {
		while (true)
		{
			BOOST_TEST(simu(error) == '0');
			co_await OnClk(clkSlow);
		}
	});

	design.postprocess();

	vhdl::VHDLExport vhdl("export/cdc_gatemate_fifo_hw_test/cdc_gatemate_fifo_hw_test.vhd");
	vhdl(design.getCircuit());

	auto flowTask = std::async([]() {
		return std::system("make -C export/cdc_gatemate_fifo_hw_test");
		});

	runHitsTimeout({ 4, 1'000'000 });
	BOOST_TEST(flowTask.get() == 0);
}


