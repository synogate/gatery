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

#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/arch/colognechip/SpecialFunction.h>
#include <gatery/scl/arch/colognechip/Bitstream.h>

#include <gatery/scl/stream/strm.h>
#include <gatery/scl/io/TxRxPacketStreamRouter.h>
#include <gatery/scl/io/usb/GpioPhyCdc.h>

using namespace gtry;
using namespace gtry::scl::strm;
using namespace gtry::scl::arch::colognechip;

BOOST_FIXTURE_TEST_CASE(arch_colognechip_switch_spi_user_mode, BoostUnitTestSimulationFixture)
{
	Clock clk12{ ClockConfig{
		.absoluteFrequency = 10_mhz,
		.name = "refclk",
		.resetType = hlim::RegisterAttributes::ResetType::NONE
	} };

	ClockScope clkScp(clk12);

	std::vector<uint8_t> bitstream = BitstreamWriter{}
		(CmdPath::enable)
		(CmdChgStatus{
			.configurationDone = 1,
			.stopConfiguration = 1,
		//	.reconfigurationEnable = 1,
		//	.enableCPEConfiguration = 1,
			.CPEReset = 1,

			.configurationMode = 0,
			.selectConfigurationMode = 1,

			.enableGPIOBankS3 = 0,
			.enableGPIOBankN2 = 1,
			.enableGPIOBankW2 = 1,
		}, 3)
		.finish();
		
	Memory<BVec> configRom(bitstream.size(), 8_b);
	configRom.fillPowerOnState(sim::createDefaultBitVectorState(std::as_bytes(std::span(bitstream))));

	CC_CFG_CTRL cfgCtrl;
	
	iota(ConstUInt(0, configRom.addressWidth())) |
		transform<InsertPos::attach>([&](const UInt& addr) { return scl::Eop{ addr == bitstream.size() - 1 }; }) |
		stall(!scl::flag(scl::Counter{1s}.isLast(), '0')) |
		transform<InsertPos::attach>([](const scl::Eop& eop, const scl::Valid& val) { return scl::Valid{ val.valid & !scl::flag(eop.eop & val.valid, '0') }; }) |
		lookup(configRom) |
		name("config") |
		scl::arch::colognechip::configure(cfgCtrl);

	pinOut(scl::Counter{ 1_b }.isLast(), "CLK0");
	pinOut(scl::Counter{ 1_b }.isLast(), "CLK1");

	design.postprocess();

	vhdl::VHDLExport vhdl("export/arch_colognechip_switch_spi_user_mode/top.vhd");
	vhdl(design.getCircuit());

	std::system("make -C export/arch_colognechip_switch_spi_user_mode jtag");

	//BOOST_TEST(!runHitsTimeout(3ms));
}

BOOST_FIXTURE_TEST_CASE(arch_colognechip_first_stage_export, BoostUnitTestSimulationFixture)
{
	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12_mhz,
		.name = "CLK12M",
		.resetType = hlim::RegisterAttributes::ResetType::NONE,
		.memoryResetType = ClockConfig::ResetType::NONE,
	} };

	Clock usbclk = pll(clk12, {
		.absoluteFrequency = 36_mhz,
		.resetType = hlim::RegisterAttributes::ResetType::NONE,
		.memoryResetType = ClockConfig::ResetType::NONE,
	});

	Clock sysclk = clkdivRegChain(usbclk, 1);

	ClockScope clkScp(sysclk);

	scl::usb::Function usbFunction;
	std::shared_ptr phy = std::make_shared<scl::usb::GpioPhyCdc>();
	{
		scl::TxRxPacketStreamRouter hostRouter;

		{
			Area ent{ "selftest", true };

			scl::TxRxPacketStream loopbackWithId{ {8_b}, {8_b} };
			loopbackWithId.tx <<= *loopbackWithId.rx;
			IF(valid(loopbackWithId.tx) & sop(loopbackWithId.tx))
				*loopbackWithId.tx = size_t('A');
			hostRouter.attach(0, move(loopbackWithId));
		}

		{
			Area ent{ "flash", true };

			scl::TxRxStream flash{ {8_b}, {8_b} };
			flash.tx <<=
				scl::SpiMaster{}
				.pin("FLASH_CLK", "FLASH_MISO", "FLASH_MOSI")
				.clockDiv(1)
				.generate(move(*flash.rx)) |
				scl::strm::fifo(1024);
			HCL_NAMED(flash);
			hostRouter.attach(2, move(flash));

			scl::TxRxStream flashCtrl{ {8_b}, {8_b} };
			Bit chipSelect = scl::capture((**flashCtrl.rx)[0], Bit{'1'}, transfer(*flashCtrl.rx));
			pinOut(chipSelect, "FLASH_CSn");

			ready(*flashCtrl.rx) = '1';
			valid(flashCtrl.tx) = '0';
			hostRouter.attach(3, move(flashCtrl));
		}

		{
			Area ent{ "reconfig", true };
			CC_CFG_CTRL cfgCtrl;

			scl::TxRxPacketStream reconfig{ {8_b}, {8_b} };
			configure(move(*reconfig.rx), cfgCtrl);
			valid(reconfig.tx) = '0';
			hostRouter.attach(4, move(reconfig));

			Bit timeout = scl::Counter(2s).isLast() & usbFunction.configuration() == 0 | name("timeout");
			cfgCtrl.recfg() = timeout;

			scl::TxRxStream reconfigCtrl{ {8_b}, {8_b} };
			cfgCtrl.recfg() |= valid(*reconfigCtrl.rx) & **reconfigCtrl.rx == 0;
			ready(*reconfigCtrl.rx) = '1';
			valid(reconfigCtrl.tx) = '0';
			hostRouter.attach(5, move(reconfigCtrl));
		}

		phy->functionClock(sysclk);

		scl::TxRxStream usb = *scl::usb::streamToSerialAdapter(usbFunction, {.phy = phy.get(), .usbClk = usbclk, .pid = 1}) | name("usb");
		usb <<= hostRouter.generate() | scl::replaceEopByLengthHeader(256);
	}

	design.postprocess();

	vhdl::VHDLExport vhdl("export/arch_colognechip_first_stage_export/top.vhd");
	vhdl(design.getCircuit());

	//BOOST_TEST(!runHitsTimeout(3ms));
}
