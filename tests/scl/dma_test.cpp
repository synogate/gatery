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
#include <gatery/scl/sim/SimuPinnedHostMemoryBuffer.h>
#include <gatery/scl/io/pci/PciToTileLink.h>

#include <gatery/scl/platform/Host.h>
#include <gatery/scl/driver/memoryBuffer/DMAFetchDepositToAxi.h>
#include <gatery/scl/driver/memoryBuffer/DMADeviceMemoryBuffer.h>
#include <gatery/scl/sim/SimuPinnedHostMemoryBuffer.h>

#include <gatery/simulation/simProc/SimulationFiber.h>

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

	scl::TileLinkUB slaveTL = scl::pci::makePciMasterCheapBurst(hostModel.requesterInterface(512_b), {}, 4_b, 48_b); //4 bits are enough to hold the number 10, which is the required logByteSize for 1kiB burst transfer

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
		.memorySize = { BitWidth(8ull << 48) }, //48 byte-addressable bits of sparse memory space
	};
	axiMemorySimulationCreateMemory(cfg);
	scl::Axi4& slaveAxi = axiMemorySimulationPort(cfg);

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

		auto &axiStorage = gtry::getSimData<hlim::MemoryStorageSparse>("axiMemory");

		BOOST_TEST(axiStorage.read(destStartAddress * 8, 1024 * 8) == hostModel.memory().read(hostByteAddressStart * 8, 1024 * 8));
		stopTest();

	});

	design.postprocess();
	if(false) recordVCD("dut.vcd");
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


struct DmaControl {
	scl::RvStream<scl::AxiToStreamCmd> depositCmd;
	scl::RvStream<scl::TileLinkStreamFetch::Command> fetchCmd;
	gtry::Reverse<scl::AxiTransferReport> axiReport;
};

BOOST_HANA_ADAPT_STRUCT(DmaControl, depositCmd, fetchCmd, axiReport);

struct dma_pcieHost_to_axi_slave_with_driver_mm { };
using SimMap_dma_pcieHost_to_axi_slave_with_driver = gtry::scl::driver::DynamicMemoryMap<dma_pcieHost_to_axi_slave_with_driver_mm>;


struct dma_pcieHost_to_axi_slave_with_driver : public BoostUnitTestSimulationFixture
{
	void execute(std::function<void(scl::driver::MemoryMapInterface&, hlim::MemoryStorage&, hlim::MemoryStorage&)> driverCode) {

		Clock clk({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
		ClockScope clkScp(clk);

		scl::Host host;

		DmaControl dmaControl;

		*dmaControl.depositCmd = scl::AxiToStreamCmd {
			.startAddress = 48_b,
			.endAddress = 48_b,
			.bytesPerBurst = 1024
		};
		
		*dmaControl.fetchCmd = scl::TileLinkStreamFetch::Command {
			.address = 48_b,
			.beats = 16_b, //this test needs to transfer 16 beats. Thus needing 5 bits
		};

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
			.memorySize = { BitWidth(8ull << 48) }, //48 byte-addressable bits of sparse memory space
		};
		axiMemorySimulationCreateMemory(cfg);
		scl::Axi4& slaveAxi = axiMemorySimulationPort(cfg);

		dmaControl.axiReport = scl::axiTransferAuditor(slaveAxi, BitWidth(dmaControl.depositCmd->bytesPerBurst*8));


		scl::TileLinkUB slaveTL = scl::pci::makePciMasterCheapBurst(host.addHostMemory(512_b), {}, 4_b, 48_b); //4 bits are enough to hold the number 10, which is the required logByteSize for 1kiB burst transfer
		scl::tileLinkToAxiDMA(move(dmaControl.fetchCmd), move(dmaControl.depositCmd), move(slaveTL), slaveAxi);


		scl::PackedMemoryMap memoryMap("memoryMap");
		scl::mapIn(memoryMap, dmaControl, "dma_ctrl");

		auto [memoryMapEntries, addressSpaceDesc, driverInterface] = host.addMemoryMap(memoryMap);

		//std::cout << memoryMap.getTree().physicalDescription << std::endl;

		using Map = SimMap_dma_pcieHost_to_axi_slave_with_driver;
		Map::memoryMap = scl::driver::MemoryMap(memoryMapEntries);		

		addSimulationFiber([&driverInterface, &clk, this, &host, &driverCode](){

			hlim::MemoryStorage *hostMemory;
			hlim::MemoryStorage *axiMemory;

			sim::SimulationFiber::awaitCoroutine<int>([&]()->SimFunction<int> { 
				co_await OnClk(clk); // await reset

				hostMemory = &host.simuHostMemory();
				axiMemory = &gtry::getSimData<hlim::MemoryStorageSparse>("axiMemory");
			});

			driverCode(*driverInterface, *hostMemory, *axiMemory);

			stopTest();
		});

		design.postprocess();
		if(false) recordVCD("dut.vcd");
		BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
	}
};


BOOST_FIXTURE_TEST_CASE(dma_pcieHost_to_axi_slave_test_DMAFetchDepositToAxi, dma_pcieHost_to_axi_slave_with_driver)
{
	execute([](scl::driver::MemoryMapInterface &driverInterface, hlim::MemoryStorage &hostMemory, hlim::MemoryStorage &axiMemory) {
		std::mt19937 rng(234578);

		std::uniform_int_distribution<scl::driver::PhysicalAddr> randomAddr(0, 1ull << 40);
		std::uniform_int_distribution<scl::driver::PhysicalAddr> randomSize(0, 4096);

		using Map = SimMap_dma_pcieHost_to_axi_slave_with_driver;
		Map map{};

		scl::driver::DMAFetchDepositToAxi fetchController(map.template get<"dma_ctrl">(), driverInterface, 512/8);

		std::uint64_t accessAlignment = 1024;

		for (size_t i = 0; i < 10; i++) {
			BOOST_TEST_INFO("Transfer " << i);
			auto randomness = sim::createRandomDefaultBitVectorState(randomSize(rng)*8, rng);
			auto hostAddr = randomAddr(rng) / accessAlignment * accessAlignment;
			auto deviceAddr = randomAddr(rng) / accessAlignment * accessAlignment;

			auto transferSize = (randomness.size()/8 + accessAlignment-1)/accessAlignment*accessAlignment;


			BOOST_TEST_INFO("with size " << randomness.size()/8);
			BOOST_TEST_INFO("with deviceAddr " << std::hex << deviceAddr);
			BOOST_TEST_INFO("with hostAddr " << std::hex << hostAddr);
			BOOST_TEST_INFO("with transferSize " << transferSize);

			hostMemory.write(hostAddr*8, randomness, false, {});

			sim::SimulationFiber::awaitCoroutine<int>([&]()->SimFunction<int> {
				sim::SimulationContext::current()->onDebugMessage(nullptr, (boost::format("Transfer %i register from 0x%x to 0x%x of %d bytes.") % i % hostAddr % deviceAddr % transferSize).str());
				co_return 0;
			});

			fetchController.uploadContinuousChunk(hostAddr, deviceAddr, transferSize);

			auto retrieved = axiMemory.read(deviceAddr*8, randomness.size());

			BOOST_TEST(randomness == retrieved);
		}
	});
}



BOOST_FIXTURE_TEST_CASE(dma_pcieHost_to_axi_slave_test_MemoryBuffer_Lock, dma_pcieHost_to_axi_slave_with_driver)
{
	execute([](scl::driver::MemoryMapInterface &driverInterface, hlim::MemoryStorage &hostMemory, hlim::MemoryStorage &axiMemory) {
		std::mt19937 rng(234578);

		std::uniform_int_distribution<scl::driver::PhysicalAddr> randomSize(1, 10);

		using Map = SimMap_dma_pcieHost_to_axi_slave_with_driver;
		Map map{};

		scl::sim::driver::SimuPinnedHostMemoryBufferFactory pinnedMemoryfactory(hostMemory, 0x1000'0000);
		
		scl::driver::DMAFetchDepositToAxi fetchController(map.template get<"dma_ctrl">(), driverInterface, 512/8);

		scl::driver::DummyDeviceMemoryAllocator deviceAllocator;
		scl::driver::DMAMemoryBufferFactory deviceBufferFactory(deviceAllocator, pinnedMemoryfactory, fetchController);

		for (size_t i = 0; i < 10; i++) {

			std::vector<size_t> expected;

			auto buffer = deviceBufferFactory.allocateDerived(1024);// * randomSize(rng));

			{
				auto mapped = buffer->map(scl::driver::MemoryBuffer::Flags::DISCARD);
				auto data = mapped.view<size_t>();
				for (auto &d : data)
					d = rng();

				expected = std::vector<size_t>(data.begin(), data.end());
			}

			auto retrieved = axiMemory.read(buffer->deviceAddr()*8, buffer->size()*8);
			
			bool matches = retrieved == std::as_bytes(std::span(expected));
			BOOST_TEST(matches);
		}
	});
}


BOOST_FIXTURE_TEST_CASE(dma_pcieHost_to_axi_slave_test_MemoryBuffer_Write, dma_pcieHost_to_axi_slave_with_driver)
{
	execute([](scl::driver::MemoryMapInterface &driverInterface, hlim::MemoryStorage &hostMemory, hlim::MemoryStorage &axiMemory) {
		std::mt19937 rng(234578);

		std::uniform_int_distribution<scl::driver::PhysicalAddr> randomSize(1, 10);

		using Map = SimMap_dma_pcieHost_to_axi_slave_with_driver;
		Map map{};

		scl::sim::driver::SimuPinnedHostMemoryBufferFactory pinnedMemoryfactory(hostMemory, 0x1000'0000);
		
		scl::driver::DMAFetchDepositToAxi fetchController(map.template get<"dma_ctrl">(), driverInterface, 512/8);

		scl::driver::DummyDeviceMemoryAllocator deviceAllocator;
		scl::driver::DMAMemoryBufferFactory deviceBufferFactory(deviceAllocator, pinnedMemoryfactory, fetchController);

		for (size_t i = 0; i < 10; i++) {

			std::vector<size_t> expected;

			auto buffer = deviceBufferFactory.allocateDerived(1024);// * randomSize(rng));

			expected.resize(buffer->size() / sizeof(size_t));
			for (auto &d : expected)
				d = rng();
			buffer->write(std::as_bytes(std::span(expected)));

			auto retrieved = axiMemory.read(buffer->deviceAddr()*8, buffer->size()*8);
			
			bool matches = retrieved == std::as_bytes(std::span(expected));
			BOOST_TEST(matches);
		}
	});
}
