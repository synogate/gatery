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
#include <gatery/frontend.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/io/pci/PciToTileLink.h>
#include <gatery/scl/sim/PcieHostModel.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>

#include <gatery/export/DotExport.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;


BOOST_FIXTURE_TEST_CASE(tileLink_requester_test_1word, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth tlpW = 256_b;

	scl::sim::PcieHostModel model;
	model.defaultHandlers();
	scl::pci::RequesterInterface reqInt = model.requesterInterface(tlpW);

	TileLinkUL slaveTl = scl::pci::makePciMaster(move(reqInt), 4_b, 32_b, 8_b);

	TileLinkMasterModel tlmm;
	tlmm.init("tlmm", 4_b, 32_b, 2_b, 8_b);
	auto& masterTl = (TileLinkUL&) tlmm.getLink();

	slaveTl <<= masterTl;
	
	addSimulationProcess([&, this]()->SimProcess {
		fork(model.completeRequests(clk, 3));
		co_await OnClk(clk);
		auto [value, defined, error] = co_await tlmm.get(0, 2, clk);
		BOOST_TEST(error == false);
		BOOST_TEST(defined == (1ull << 32) - 1 );
		BOOST_TEST(value == scl::sim::readState(model.memory().read(0, 32)));
		co_await OnClk(clk);
		stopTest();
	});
	if (true) { recordVCD("dut.vcd"); }
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

