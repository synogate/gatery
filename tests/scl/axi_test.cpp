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

using namespace gtry;

BOOST_FIXTURE_TEST_CASE(axi_memory_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	Memory<UInt> mem{ 1024, 16_b };
	scl::Axi4 axi = scl::Axi4::fromMemory(mem);
	pinOut(axi, "axi");

	addSimulationProcess([&]()->SimProcess {
		simInit(axi);

		co_await scl::simPut(axi, 0, 1, 0x1234, clock);
		auto [data, def, err] = co_await scl::simGet(axi, 0, 1, clock);

		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(axi_memory_simulation_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::Axi4& axi = scl::axiMemorySimulation(scl::AxiMemorySimulationConfig{
		.axiCfg = scl::AxiConfig{.addrW = 16_b, .dataW = 16_b, .wUserW = 2_b, .rUserW = 2_b }
	});
	pinOut(axi, "axi");

	addSimulationProcess([&]()->SimProcess {
		simInit(axi);

		{
			co_await scl::simPut(axi, 0, 1, 0x1234, clock);
			auto [data, def, err] = co_await scl::simGet(axi, 0, 1, clock);
			BOOST_TEST(!err);
			BOOST_TEST(def != 0);
			BOOST_TEST(data == 0x1234);
		}

		{
			co_await scl::simPut(axi, 8, 3, 0x1234'5678'90AB'CDEFull, clock);
			auto [data, def, err] = co_await scl::simGet(axi, 8, 3, clock);
			BOOST_TEST(!err);
			BOOST_TEST(~def == 0);
			BOOST_TEST(data == 0x1234'5678'90AB'CDEFull);
		}

		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(axi_axiGenerateAddressFromCommand_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::RvStream<scl::AxiToStreamCmd> axiToStreamCmdStream{ { 
			.startAddress = 16_b,
			.endAddress = 16_b,
			.bytesPerBurst = 64
	} };
	pinIn(axiToStreamCmdStream, "axiToStreamCmdStream");

	scl::RvStream<scl::AxiAddress> axiAddressStream = 
		scl::axiGenerateAddressFromCommand(move(axiToStreamCmdStream), { .addrW = 16_b, .dataW = 16_b });
	pinOut(axiAddressStream, "axiAddressStream");

	addSimulationProcess([&]()->SimProcess {
		simu(valid(axiToStreamCmdStream)) = '0';

		co_await OnClk(clock);
		simu(axiToStreamCmdStream->startAddress) = 128;
		simu(axiToStreamCmdStream->endAddress) = 1024;

		for (size_t i = 0; i < 3; ++i)
			co_await scl::performTransfer(axiToStreamCmdStream, clock);
	});

	addSimulationProcess([&]()->SimProcess {
		simu(ready(axiAddressStream)) = '1';

		for (size_t i = 128; i < 1024; i += axiToStreamCmdStream->bytesPerBurst)
		{
			co_await scl::performTransferWait(axiAddressStream, clock);
			BOOST_TEST(simu(axiAddressStream->addr) == i);
		}

		for (size_t i = 128; i < 1024; i += axiToStreamCmdStream->bytesPerBurst)
		{
			co_await OnClk(clock);
			BOOST_TEST(simu(valid(axiAddressStream)) == '1');
			BOOST_TEST(simu(axiAddressStream->addr) == i);
		}

		fork(scl::strm::readyDriverRNG(axiAddressStream, clock, 50));

		for (size_t i = 128; i < 1024; i += axiToStreamCmdStream->bytesPerBurst)
		{
			co_await scl::performTransferWait(axiAddressStream, clock);
			BOOST_TEST(simu(axiAddressStream->addr) == i);
		}

		for (size_t i = 0; i < 4; ++i)
		{
			co_await OnClk(clock);
			BOOST_TEST(simu(valid(axiAddressStream)) == '0');
		}

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(axi_dma_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::RvStream<scl::AxiToStreamCmd> axiToStreamCmd{ {
			.startAddress = 8_b,
			.endAddress = 8_b,
			.bytesPerBurst = 16
	} };
	pinIn(axiToStreamCmd, "axiToStreamCmd");

	scl::RvStream<scl::AxiToStreamCmd> axiFromStreamCmd{ {
		.startAddress = 8_b,
		.endAddress = 8_b,
		.bytesPerBurst = 16
	} };
	pinIn(axiFromStreamCmd, "axiFromStreamCmd");

	Memory<BVec> mem{ 256, 16_b };
	std::vector<uint16_t> memData;
	for (uint16_t i = 0; i < 8; ++i)
		memData.push_back(i);
	mem.fillPowerOnState(sim::createDefaultBitVectorState(memData.size() * 16, memData.data()));

	scl::Axi4 axi = scl::Axi4::fromMemory(mem);
	scl::axiDMA(move(axiToStreamCmd), move(axiFromStreamCmd), axi, 2);
	HCL_NAMED(axi); tap(axi);

	// generate circuit to check memory contents
	scl::RvStream<scl::AxiToStreamCmd> checkCmd{ {
		.startAddress = 8_b,
		.endAddress = 8_b,
		.bytesPerBurst = 16
	} };
	pinIn(checkCmd, "checkCmd");

	scl::Axi4 checkAxi = scl::Axi4::fromMemory(mem);
	scl::Stream checkOut = scl::axiToStream(move(checkCmd), checkAxi);
	pinOut(checkOut, "checkOut");
	axiDisableWrites(checkAxi);

	addSimulationProcess([&]()->SimProcess {
		simu(valid(axiToStreamCmd)) = '0';
		co_await OnClk(clock);

		simu(axiToStreamCmd->startAddress) = 0;
		simu(axiToStreamCmd->endAddress) = 16;
		while (true)
			co_await scl::performTransfer(axiToStreamCmd, clock);
	});

	addSimulationProcess([&]()->SimProcess {
		simu(valid(axiFromStreamCmd)) = '0';
		co_await OnClk(clock);

		for (size_t i = 0; i < 3; ++i)
		{
			simu(axiFromStreamCmd->startAddress) = (i + 1) * 16;
			simu(axiFromStreamCmd->endAddress) = (i + 2) * 16;
			co_await scl::performTransfer(axiFromStreamCmd, clock);
		}
	});

	addSimulationProcess([&]()->SimProcess {
		simu(valid(checkCmd)) = '0';
		simu(ready(checkOut)) = '1';
		for(size_t i = 0; i < 4; ++i)
			co_await OnClk(clock);

		simu(checkCmd->startAddress) = 16;
		simu(checkCmd->endAddress) = 64;
		simu(valid(checkCmd)) = '1';

		for (size_t i = 0; i < 3; ++i)
		{
			for (size_t j = 0; j < 8; ++j)
			{
				co_await scl::performTransferWait(checkOut, clock);
				BOOST_TEST(simu(*checkOut) == j);
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}
