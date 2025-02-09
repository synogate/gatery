/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software;
 you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation;
 either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY;
 without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library;
 if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "scl/pch.h"
#include <future>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/scl/arch/colognechip/io.h>
#include <gatery/scl/io/DisplaySync.h>

using namespace gtry;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(tmdsReduction, gtry::BoostUnitTestSimulationFixture)
{
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

BOOST_FIXTURE_TEST_CASE(DisplayModeParser_test, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope scope(clock);

	std::istringstream modeStr{
		"25.175 16 18 20 22 4 6 8 10 -hsync -vsync"
	};
	scl::DisplayMode mode;
	modeStr >> mode;
	HCL_NAMED(mode);

	BOOST_TEST(mode.pixelFreq == hlim::ClockRational{ 25'175'000 });
	addSimulationProcess([&]()->SimProcess {
		BOOST_TEST(simu(mode.w.resolution) == 16);
		BOOST_TEST(simu(mode.w.syncStart) == 18);
		BOOST_TEST(simu(mode.w.syncEnd) == 20);
		BOOST_TEST(simu(mode.w.total) == 22);
		BOOST_TEST(simu(mode.h.resolution) == 4);
		BOOST_TEST(simu(mode.h.syncStart) == 6);
		BOOST_TEST(simu(mode.h.syncEnd) == 8);
		BOOST_TEST(simu(mode.h.total) == 10);
		co_return;
	});

	design.postprocess();
	runTicks(clock.getClk(), 4);
}

BOOST_FIXTURE_TEST_CASE(DisplaySync_test, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope scope(clock);

	std::istringstream modeStr{
		"25.175 16 18 20 22 4 6 8 10 -hsync -vsync"
	};
	scl::DisplayMode mode;
	modeStr >> mode;
	HCL_NAMED(mode);

	scl::DisplaySync sync;
	sync.init(mode);
	pinOut(sync, "sync");

	addSimulationProcess([&]()->SimProcess {
		while (simu(sync.onScreen) == '0')
			co_await OnClk(clock);

		for(size_t line = 0; line < 11; ++line)
		{
			char vsyncState = (line < 6 || line >= 8) ? '0' : '1';

			// line data
			for (size_t i = 0; i < 16; ++i)
			{
				if (line < 4 || line == 10)
				{
					BOOST_TEST(simu(sync.onScreen) == '1');
					BOOST_TEST(simu(sync.x) == i);
				}
				else
				{
					BOOST_TEST(simu(sync.onScreen) == '0');
				}

				BOOST_TEST(simu(sync.hsync) == '0');
				BOOST_TEST(simu(sync.vsync) == vsyncState);
				co_await OnClk(clock);
			}

			// post sync
			for (size_t i = 0; i < 2; ++i)
			{
				BOOST_TEST(simu(sync.onScreen) == '0');
				BOOST_TEST(simu(sync.hsync) == '0');
				BOOST_TEST(simu(sync.vsync) == vsyncState);
				co_await OnClk(clock);
			}

			// hsync
			for (size_t i = 0; i < 2; ++i)
			{
				BOOST_TEST(simu(sync.onScreen) == '0');
				BOOST_TEST(simu(sync.hsync) == '1');
				BOOST_TEST(simu(sync.vsync) == vsyncState);
				co_await OnClk(clock);
			}

			// presync
			for (size_t i = 0; i < 2; ++i)
			{
				BOOST_TEST(simu(sync.onScreen) == '0');
				BOOST_TEST(simu(sync.hsync) == '0');
				BOOST_TEST(simu(sync.vsync) == vsyncState);
				co_await OnClk(clock);
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 4, 1'000'000 }));
}


static Bit gateMate_ddr_serdes(scl::VStream<BVec> in)
{
	Area ent{ "gateMate_ddr_serdes", true };

	BVec shiftReg = in->width();
	shiftReg >>= 2;
	IF(valid(in))
		shiftReg = *in;
	shiftReg = reg(shiftReg) | name("shiftReg");

	scl::arch::colognechip::CC_ODDR ddr;
	//ddr.clockInversion(true);
	ddr.D0() = shiftReg[0];
	ddr.D1() = reg(shiftReg[1]);

	return ddr.Q();
}

static void gateMate_lvds(Bit signal, std::string_view name)
{
	scl::arch::colognechip::CC_LVDS_OBUF buf;
	buf.voltage("1.8");
	buf.boost(true);
	buf.regOut(true);
	buf.pin(std::string{name} + "p", std::string{name} + "n");
	buf.O() = reg(signal);
}

static Bit gateMate_serdes(BVec data, const Clock& slowClk, const Clock& fastClk)
{
	Area ent{ "scl_serdes", true };

	Bit slowEdge = scl::synchronizeEvent('1', slowClk, fastClk) | name("slowEdge");
	BVec inWord = reg(data, { .clock = slowClk }) | name("inWord");

	BVec shiftReg = data.width();
	shiftReg >>= 1;
	IF(slowEdge)
		shiftReg = allowClockDomainCrossing(inWord, slowClk, fastClk);
	shiftReg = reg(shiftReg, { .clock = fastClk }) | name("shiftReg");

	scl::arch::colognechip::CC_ODDR ddr;
	ddr.D0() = shiftReg[1];
	ddr.D1() = shiftReg[0];
	return ddr.Q();

	return shiftReg.lsb();
}

static Clock gateMate_pll(const Clock& in, hlim::ClockRational freq)
{
	ExternalModule pll{ "CC_PLL" };
	pll.requiresComponentDeclaration(true);
	pll.isEntity(false);
	pll.generic("REF_CLK") = std::to_string(hlim::toDouble(in.absoluteFrequency() / hlim::ClockRational{ 1'000'000 }));
	pll.generic("OUT_CLK") = std::to_string(hlim::toDouble(freq / hlim::ClockRational{ 1'000'000 }));
	//pll.generic("PERF_MD") = "LOWPOWER";
	pll.generic("LOW_JITTER") = 1;
	pll.generic("CI_FILTER_CONST") = 2;
	pll.generic("CP_FILTER_CONST") = 4;
	pll.generic("LOCK_REQ") = 1;
	pll.clockIn(in, "CLK_REF");

	Bit clk0 = pll.out("CLK0");
	ExternalModule clk0BufG{ "CC_BUFG" };
	clk0BufG.requiresComponentDeclaration(true);
	clk0BufG.isEntity(false);
	clk0BufG.in("I") = clk0;

	Clock clock = clk0BufG.clockOut("O", {}, ClockConfig{
		.absoluteFrequency = freq,
		.resetType = hlim::RegisterAttributes::ResetType::NONE,
		.memoryResetType = hlim::RegisterAttributes::ResetType::NONE
	});

	return clock;
}

static Clock gateMate_clockDiv10()
{
	Area ent{ "scl_clockDiv10", true };

	Bit toggle;
	toggle = reg(toggle ^ scl::Counter(5).isLast(), '0') | name("toggle");

	ExternalModule clk0BufG{ "CC_BUFG" };
	clk0BufG.requiresComponentDeclaration(true);
	clk0BufG.isEntity(false);
	clk0BufG.in("I") = toggle;
	
	Clock clock = clk0BufG.clockOut("O", {}, ClockConfig{
		.absoluteFrequency = ClockScope::getClk().absoluteFrequency() * hlim::ClockRational{ 1, 10 },
		.resetType = hlim::RegisterAttributes::ResetType::NONE,
		.memoryResetType = hlim::RegisterAttributes::ResetType::NONE
	});

	//Clock clock = ClockScope::getClk().deriveClock({ 
	//	.frequencyMultiplier = hlim::ClockRational{ 1, 10 } 
	//});
	//clock.overrideClkWith(toggle);
	return clock;
}

static Clock gateMate_clockDiv5()
{
	Area ent{ "scl_clockDiv5", true };

	Clock risingClk = ClockScope::getClk();
	Clock fallingClk = risingClk.deriveClock({ .triggerEvent = ClockConfig::TriggerEvent::FALLING });

	ClockScope rising{ risingClk };
	scl::Counter ctr1(5);
	Bit clk1 = scl::flag(ctr1.isLast(), ctr1.value() == 1) | name("clk1");

	ClockScope falling{ fallingClk };
	scl::Counter ctr2(5);
	Bit clk2 = scl::flag(ctr2.isLast(), ctr2.value() == 1) | name("clk2");

	ExternalModule clk0BufG{ "CC_BUFG" };
	clk0BufG.requiresComponentDeclaration(true);
	clk0BufG.isEntity(false);
	clk0BufG.in("I") = clk1 | clk2;

	Clock clock = clk0BufG.clockOut("O", {}, ClockConfig{
		.absoluteFrequency = ClockScope::getClk().absoluteFrequency() * hlim::ClockRational{ 1, 10 },
		.resetType = hlim::RegisterAttributes::ResetType::NONE,
		.memoryResetType = hlim::RegisterAttributes::ResetType::NONE
	});
	return clock;
}


BOOST_FIXTURE_TEST_CASE(basicPixelPipeline_test, gtry::BoostUnitTestSimulationFixture)
{
	std::istringstream modeStr{ 
//		"25.175 640 656 752 800 480 490 492 525 -hsync -vsync" 
		"25.175 640 648 760 800 480 482 484 525 -hsync -vsync" 
	};
	scl::DisplayMode mode;
	modeStr >> mode;
	HCL_NAMED(mode);

	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12'000'000,
		.name = "CLK12M",
		.resetType = hlim::RegisterAttributes::ResetType::NONE
	} };

	// we use ddr output to reduce out bit clock to 5 * pixel clock
	hlim::ClockRational bitFreq = mode.pixelFreq * hlim::ClockRational{ 10, 2 };
	Clock clkFast = gateMate_pll(clk12, bitFreq);
	ClockScope scopeFast(clkFast);
	Clock clkSlow = gateMate_clockDiv5();
	ClockScope scope(clkSlow);
#if 1
	// TODO: move 'modern' stream wrapper into scl
	scl::VStream<scl::DisplaySync> modeSync;
	valid(modeSync) = '1';
	ENIF(transfer(modeSync))
		modeSync->init(mode);

	HCL_NAMED(modeSync);
	modeSync = move(modeSync) 
		| scl::strm::regDownstream()
		| scl::strm::regDownstream();

	scl::Stream tmdsChan = transform(modeSync, [&](const scl::DisplaySync& sync) {
		ENIF(transfer(modeSync))
		{
			//scl::Counter phase{ 2 };
			//std::array<BVec, 3> rgb;
			//rgb[0] = mux(phase.isFirst(), { BVec{"10b1000000000"}, BVec{"10b0011111111"} });
			//rgb[1] = mux(phase.isFirst(), { BVec{"10b0100000000"}, BVec{"10b1111111111"} });
			//rgb[2] = rgb[1];
			
			std::array<UInt, 3> rgb{
				0xFF, 0xFF, 0xFF
			};
			IF(modeSync->y.upper(3_b) == 0)
				rgb[0] = modeSync->x(1, 8_b);
			IF(modeSync->y.upper(3_b) == 1)
				rgb[1] = modeSync->x(1, 8_b);
			IF(modeSync->y.upper(3_b) == 2)
				rgb[2] = modeSync->x(1, 8_b);
			IF(modeSync->x > 600 & modeSync->x < 640)
			{
				rgb[0] = 0;
				rgb[1] = 0;
				rgb[2] = 0;
			}
			//rgb[1] = modeSync->y.lower(8_b);
			//rgb[2] = modeSync->x.upper(8_b);
			//rgb[2] = zext(modeSync->y.upper(2_b));
			//IF(modeSync->x < 128)
				//rgb[1] = modeSync->x.lower(8_b);


			// initialize to idle symbol
			std::array<BVec, 3> tmdsCode{
				BVec{"10b1101010100"},
				BVec{"10b1101010100"},
				BVec{"10b1101010100"}
			};

			// override ctrl codes
			tmdsCode[0] = mux(cat(modeSync->vsync, modeSync->hsync), {
				BVec{"10b1101010100"}, // idle
				BVec{"10b0010101011"}, // hsync
				BVec{"10b0101010100"}, // vsync
				BVec{"10b1010101011"}  // both
			});

			// set color
			IF(modeSync->onScreen)
				for (size_t i = 0; i < tmdsCode.size(); ++i)
					tmdsCode[i] = (BVec)scl::hdmi::tmdsEncodeSymbol(rgb[2 - i]);

			// TODO: find mistake
			for (size_t i = 0; i < 3; ++i)
				tmdsCode[i] = ~tmdsCode[i];

			return tmdsCode;
		}
		throw;
	}) | scl::strm::attach(*modeSync) | name("tmds");

	tmdsChan = reg(tmdsChan);
	tmdsChan = reg(tmdsChan);
	//tmdsChan = reg(tmdsChan);


	ClockScope ioScope{ clkFast };
	//for (size_t i = 0; i < 3; ++i)
	//{
	//	Bit d = gateMate_serdes(tmdsChan->at(i), clkSlow, clkFast);
	//	gateMate_lvds(d, "HDMI_D" + std::to_string(i));
	//}
	//{
	//	Bit d = gateMate_serdes(
	//		(BVec)sext(scl::strm::get<scl::DisplaySync>(tmdsChan).onScreen, 10_b), 
	//		clkSlow, clkFast
	//	);
	//	pinOut(d, "UART0_TX");
	//}
#endif
	{
		ClockScope ioScope{ clkFast };
		Bit clockSer = gateMate_serdes(BVec{ "10b1111100000" }, clkSlow, clkFast);
		gateMate_lvds(clockSer, "HDMI_CK");
	}
	//{
	//	ClockScope ioScope{ clkSlow };
	//	Bit toggle;
	//	toggle = reg(toggle ^ scl::Counter(25'0 / 2).isLast()) | name("toggleScl");
	//
	//	scl::arch::colognechip::CC_IOBUF scl;
	//	scl.voltage("1.8");
	//	scl.pullup(true);
	//	scl.pin("HDMI_SCL");
	//	scl.disable() = '0';
	//	scl.O() = toggle;
	//
	//	scl::arch::colognechip::CC_IOBUF sda;
	//	sda.voltage("1.8");
	//	sda.pullup(true);
	//	sda.pin("HDMI_SDA");
	//	sda.disable() = '1';
	//}

	// https://www.rapidtables.com/convert/color/hsv-to-rgb.html
	/*
	function hsvToRgb(h, s, v) { 
		let r, g, b;
		h /= 360, s /= 100, v /= 100;

		let i = Math.floor(h * 6);
		let f = h * 6 - i;
		let p = v * (1 - s);
		let q = v * (1 - f * s);
		let t = v * (1 - (1 - f) * s);

		switch (i % 6) { 
			case 0: r = v, g = t, b = p; break;
			case 1: r = q, g = v, b = p; break;
			case 2: r = p, g = v, b = t; break;
			case 3: r = p, g = q, b = v; break;
			case 4: r = t, g = p, b = v; break;
			case 5: r = v, g = p, b = q; break;
		} 
		return[Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
	 }
	*/

	design.postprocess();

	vhdl::VHDLExport vhdl("export/basicPixelPipeline_test/basicPixelPipeline_test.vhd");
	vhdl(design.getCircuit());

	auto flowTask = std::async([]() {
		return std::system("make -C export/basicPixelPipeline_test");
	});

	BOOST_TEST(!runHitsTimeout({ 4, 1'000'000 }));
	flowTask.wait();
}
