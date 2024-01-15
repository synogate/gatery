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

//#include <gatery/scl/tilelink/tilelink.h>
//#include <gatery/scl/tilelink/TileLinkStreamFetch.h>
//#include <gatery/scl/tilelink/TileLinkDMA.h>

#include <gatery/scl/sim/PcieHostModel.h>
#include <gatery/scl/io/pci/PciToTileLink.h>

using namespace gtry;

BOOST_FIXTURE_TEST_CASE(tileLinkToAxiDMA_poc_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000, .memoryResetType = ClockConfig::ResetType::NONE });
	ClockScope clkScp(clock);

	scl::RvStream<scl::AxiToStreamCmd> depositCmd{ {
			.startAddress = 8_b,
			.endAddress = 8_b,
			.bytesPerBurst = 16 // not sure about this yet
		} };
	pinIn(depositCmd, "depositCmd");

	scl::RvStream<scl::TileLinkStreamFetch::Command> fetchCommand{ {
			.address = 8_b,
			.beats = 8_b, //not sure about this
		} };
	pinIn(fetchCommand, "fetchCommand");

	scl::sim::PcieHostModel hostStorage();

	std::shared_ptr<hlim::MemoryStorageDense> axiStorage = std::make_shared<hlim::MemoryStorageDense>(8 * 64);

	scl::AxiMemorySimulationConfig cfg{
		.axiCfg{
			.addrW = 8_b,
			.dataW = 8_b,
			.idW = 0_b,
			.arUserW = 0_b,
			.awUserW = 0_b,
			.wUserW = 0_b,
			.bUserW = 0_b,
			.rUserW = 0_b,
		},
		.storage = axiStorage,
	};


	auto axiMemModel = 





/*

	addSimulationProcess([&]()->SimProcess {
		simu(valid(cmd)) = '0';
		co_await OnClk(clock);

		for (size_t i = 1; i < 4; ++i)
		{
			simu(cmd->startAddress) = i * 16;
			simu(cmd->endAddress) = i * 32;
			co_await performTransfer(cmd, clock);
		}
		});

	addSimulationProcess([&]()->SimProcess {
		simu(valid(data)) = '0';

		for(size_t i = 0;; ++i)
		{
			simu(*data) = i;
			co_await performTransfer(data, clock);
		}
		});

	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(link.a, clock, 80));

		for (size_t i = 0; i < 6*8; ++i)
		{
			co_await performTransferWait(link.a, clock);
			BOOST_TEST(simu(link.a->data) == i);
		}
		co_await OnClk(clock);
		BOOST_TEST(!simu(valid(link.a)));

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
	*/
}


