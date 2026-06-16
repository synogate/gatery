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
