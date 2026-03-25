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
#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/io/DisplayEngine.h>
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

BOOST_FIXTURE_TEST_CASE(basicGateMateHDMI_test, gtry::BoostUnitTestSimulationFixture, *boost::unit_test::disabled())
{
	// https://tomverbeure.github.io/video_timings_calculator
	std::istringstream modeStr{ 
//		"25.175 640 656 752 800 480 490 492 525 -hsync -vsync" 
		"25.175 640 648 760 800 480 482 484 525 -hsync -vsync" 
//		"29.988 1280 1288 1320 1360 720 721 729 735 +HSync -VSync"
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
	
	Clock clkFast = scl::arch::colognechip::pll(clk12, { .absoluteFrequency = bitFreq, .resetType = ClockConfig::ResetType::NONE });
	Clock clkSlow = scl::arch::colognechip::clkdiv(clkFast, { .absoluteFrequency = mode.pixelFreq, .resetType = ClockConfig::ResetType::NONE });
	ClockScope scope(clkSlow);

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

			return tmdsCode;
		}
		throw;
	}) | scl::strm::attach(*modeSync) | name("tmds");

	tmdsChan = reg(tmdsChan);
	tmdsChan = reg(tmdsChan);
	//tmdsChan = reg(tmdsChan);

	ClockScope ioScope{ clkFast };
	for (size_t i = 0; i < 3; ++i)
	{
		scl::arch::colognechip::CC_LVDS_OBUF buf;
		//buf.boost(true);

		std::string name = "HDMI_D" + std::to_string(i);
		// not that "n" and "p" are swapped. this is because TMDS is logically inverted. (pulling D+ to ground is a logic high)
		buf.pin(name + "n", name + "p");
		buf.O() = scl::arch::colognechip::serdes(tmdsChan->at(i), clkSlow, clkFast);
	}
	{
		ClockScope ioScope{ clkSlow };
		scl::arch::colognechip::CC_LVDS_OBUF buf;
		buf.pin("HDMI_CKn", "HDMI_CKp");
		buf.O() = clkSlow.clkSignal();
	}
	
#if 0 // basic DDC channel level shifter test
	{
		ClockScope ioScope{ clkSlow };
		Bit toggle;
		toggle = reg(toggle ^ scl::Counter(25'0 / 2).isLast()) | name("toggleScl");
	
		scl::arch::colognechip::CC_IOBUF scl;
		scl.voltage("1.8");
		scl.pullup(true);
		scl.pin("HDMI_SCL");
		scl.disable() = toggle;
		scl.O() = '0';

		scl::arch::colognechip::CC_IOBUF sda;
		sda.voltage("1.8");
		sda.pullup(true);
		sda.pin("HDMI_SDA");
		sda.disable() = toggle;
		sda.O() = '0';
	}
#endif

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

	runHitsTimeout({ 4, 1'000'000 });
	flowTask.wait();
}

BOOST_FIXTURE_TEST_CASE(basicGateMateLVDS_test, gtry::BoostUnitTestSimulationFixture, *boost::unit_test::disabled())
{
	BitWidth colorDepth = 8_b;

#if 1
	scl::DisplayMode mode = {
		.pixelFreq = 22_mhz,
		.w = {.resolution = 1920 / 4, .syncStart = 1920 / 4, .syncEnd = 0, .total = 2130 / 4, },
		.h = {.resolution = 1080, .syncStart = 1080, .syncEnd = 0, .total = 1100, },
	};
#elif 0
	// typical datasheet values
	scl::DisplayMode mode = {
		.pixelFreq = 27_mhz,
		.w = { .resolution = 1600/2, .syncStart = 1600/2, .syncEnd = 0, .total = 1940/2, },
		.h = { .resolution = 900, .syncStart = 900, .syncEnd = 0, .total = 926, },
	};
#else
	// min datasheet values
	scl::DisplayMode mode = {
		.pixelFreq = 12_mhz,
		.w = { .resolution = 1600 / 2, .syncStart = 1600 / 2, .syncEnd = 0, .total = 2000 / 2, },
		.h = { .resolution = 900, .syncStart = 900, .syncEnd = 0, .total = 932, },
	};
#endif
	HCL_NAMED(mode);
	
	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12'000'000,
		.name = "CLK12M",
		.resetType = hlim::RegisterAttributes::ResetType::NONE
	} };
	
	hlim::ClockRational bitFreq = mode.pixelFreq * hlim::ClockRational{ 7, 1 };
	
	Clock clkFast = scl::arch::colognechip::pll(clk12, { .absoluteFrequency = bitFreq, .resetType = ClockConfig::ResetType::NONE });
	Clock clkSlow = scl::arch::colognechip::clkdiv(clkFast, { .absoluteFrequency = mode.pixelFreq, .resetType = ClockConfig::ResetType::NONE });
	ClockScope scope(clkSlow);

	// TODO: move 'modern' stream wrapper into scl
	scl::VStream<scl::DisplaySync> modeSync = 
		scl::displaySync(mode) 
		| scl::strm::remove(scl::Ready{ '1' }) 
		| name("modeSync") 
		| scl::strm::regDownstream()
		;

	scl::Stream rgbImage = transform(modeSync, [&](const scl::DisplaySync& sync) {
		ENIF(transfer(modeSync))
		{
			std::array<UInt, 3> rgb{
				0xFF, 0xFF, 0xFF
			};
			UInt grad = modeSync->x.lower(8_b);
			UInt sel = modeSync->y.upper(3_b);

			IF(sel == 0)
				rgb[0] = grad;
			IF(sel == 1)
				rgb[1] = grad;
			IF(sel == 2)
				rgb[2] = grad;

			IF(sel > 3)
				for (auto& c : rgb) c = 0;
			IF(sel == 4)
				rgb[0] = grad;
			IF(sel == 5)
				rgb[1] = grad;
			IF(sel == 6)
				rgb[2] = grad;

			IF(modeSync->x > 600 & modeSync->x < 640)
			{
				rgb[0] = 0;
				rgb[1] = 0;
				rgb[2] = 0;
			}
			return rgb;
		}
		throw;
	})	| scl::strm::attach(*modeSync) 
		| scl::strm::regDownstream()
		| name("rgbImage");

	std::deque<BVec> lvds = scl::displayOpenLDIEncode(
		{ rgbImage->at(0).upper(colorDepth), rgbImage->at(1).upper(colorDepth), rgbImage->at(2).upper(colorDepth) },
		get<scl::DisplaySync>(rgbImage).onScreen,
		get<scl::DisplaySync>(rgbImage).onScreen,	// vsync
		get<scl::DisplaySync>(rgbImage).onScreen	// hsync (both ignored by flat panels, so output onScreen to reduce toggle rate)
	);

	// output pixel twice per channel to allow even divider and therefore ddr output usage in serdes
	// this results in a total of 4 pixel per cycle
	std::deque<BVec> lvds_double;
	for (BVec& chan : lvds)
		lvds_double.push_back(cat(chan, chan));
	HCL_NAMED(lvds_double);

	scl::displayOpenLDIGateMate(clkFast, lvds_double, "DISPLAY_RXO");
	scl::displayOpenLDIGateMate(clkFast, lvds_double, "DISPLAY_RXE");

	{ // back light code
		ClockScope slowScope{ clkSlow };

		scl::Counter pwmLevel(8_b);
		IF(scl::Counter{ 20ms }.isLast())
			pwmLevel.inc();

		scl::BacklightCtrl bl = scl::backlightController(pwmLevel.value(), {.pwmFreq = 20_khz}) | name("backlight");
		pinOut(bl.powerEn).setName("DISPLAY_EN_12V");
		pinOut(bl.ledPwm).setName("DISPLAY_BL_PWM");
		pinOut(bl.ledEn).setName("DISPLAY_BL_EN");
	}

	design.postprocess();

	vhdl::VHDLExport vhdl("export/basicGateMateLVDS_test/top.vhd");
	vhdl(design.getCircuit());

	//auto flowTask = std::async([]() {
	//	return std::system("make -C export/basicGateMateLVDS_test");
	//	});
	//
	//runHitsTimeout({ 17, 1'000'000 });
	//flowTask.wait();
}

