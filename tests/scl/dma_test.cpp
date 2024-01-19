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

#include <gatery/scl/stream/SimuHelpers.h>

#include <gatery/scl/axi/axiMasterModel.h>
#include <gatery/scl/axi/AxiDMA.h>
#include <gatery/scl/axi/AxiMemorySimulation.h>

#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkStreamFetch.h>
#include <gatery/scl/tilelink/TileLinkDMA.h>
#include <gatery/scl/dma.h>

#include <gatery/scl/sim/PcieHostModel.h>
#include <gatery/scl/io/pci/PciToTileLink.h>

using namespace gtry;

BOOST_FIXTURE_TEST_CASE(dma_pcieHost_to_axi_slave_test, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clk);

	scl::RvStream<scl::AxiToStreamCmd> depositCmd{ {
			.startAddress = 48_b,
			.endAddress = 48_b,
			.bytesPerBurst = 1024
		} };
	pinIn(depositCmd, "depositCmd");

	scl::RvStream<scl::TileLinkStreamFetch::Command> fetchCmd{ {
			.address = 48_b,
			.beats = 5_b, //this test needs to transfer 16 beats. Thus needing 5 bits
		} };
	pinIn(fetchCmd, "fetchCommand");

	std::mt19937_64 mt64(std::random_device{}());

	uint64_t hostByteAddressStart = mt64() & 0x0000'FFFF'FFFF'FF00ull;
	scl::sim::PcieHostModel hostModel(scl::sim::RandomBlockDefinition{ hostByteAddressStart * 8, (1_KiB).bits(), 87490 }, 1ull << 48);
	hostModel.defaultHandlers();
	hostModel.updateHandler(scl::pci::TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(64, 2));

	addSimulationProcess([&]()->SimProcess { return hostModel.completeRequests(clk, 2); });

	scl::TileLinkUB slaveTL = scl::pci::makePciMasterCheapBurst(hostModel.requesterInterface(512_b), 4_b, 48_b); //4 bits are enough to hold the number 10, which is the required logByteSize for 1kiB burst transfer

	auto axiStorage = std::make_shared<hlim::MemoryStorageSparse>((uint64_t)(8ull << 48)); //48 byte-addressable bits of sparse memory space;

	scl::AxiMemorySimulationConfig cfg{
		.axiCfg{
			.addrW = 48_b,
			.dataW = 512_b,
			.idW = 0_b,
			.arUserW = 0_b,
			.awUserW = 0_b,
			.wUserW = 0_b,
			.bUserW = 0_b,
			.rUserW = 0_b,
		},
		.storage = axiStorage,
	};
	scl::Axi4& slaveAxi = axiMemorySimulation(cfg);

	scl::AxiTransferReport report = scl::axiTransferAuditor(slaveAxi, BitWidth(depositCmd->bytesPerBurst*8));
	pinOut(report, "axi_report");

	scl::tileLinkToAxiDMA(move(fetchCmd), move(depositCmd), move(slaveTL), slaveAxi);

	//send tileLink fetch command
	addSimulationProcess([&]()->SimProcess {
		simu(valid(fetchCmd)) = '0';
		co_await OnClk(clk);
		simu(fetchCmd->address) = hostByteAddressStart;
		simu(fetchCmd->beats)	= 16; // 16 beats at 512 bits is 1 kiB of transfer
		co_await performTransfer(fetchCmd, clk);
	});

	uint64_t destStartAddress = mt64() & 0x0000'FFFF'FFFF'FF00ull;
	//send axi deposit command
	addSimulationProcess([&]()->SimProcess {
		simu(valid(depositCmd)) = '0';
		co_await OnClk(clk);
		simu(depositCmd->startAddress) = destStartAddress;
		simu(depositCmd->endAddress) = destStartAddress + 1024;
		co_await performTransfer(depositCmd, clk);
	});

	//wait 30 cycles then check memories' contents
	addSimulationProcess([&]()->SimProcess {
		BOOST_TEST(simu(report.burstCount) == 0);
		BOOST_TEST(simu(report.failCount) == 0);

		while(simu(report.burstCount) != 1)
			co_await OnClk(clk);

		BOOST_TEST(simu(report.burstCount) == 1);
		BOOST_TEST(simu(report.failCount) == 0);
		BOOST_TEST(simu(report.bitsPerBurst) == (1_KiB).bits()); 

		BOOST_TEST(axiStorage->read(destStartAddress * 8, 1024 * 8) == hostModel.memory().read(hostByteAddressStart * 8, 1024 * 8));
		stopTest();

	});

	design.postprocess();
	if(false) recordVCD("dut.vcd");
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


