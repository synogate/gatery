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

#include <gatery/debug/websocks/WebSocksInterface.h>

#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/Avalon.h>
#include <gatery/scl/TL2AMM.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

scl::TileLinkUL ub2ul(scl::TileLinkUB& link)
{
	scl::TileLinkUL out;

	out.a = constructFrom(link.a);
	link.a <<= out.a;

	*out.d = constructFrom(*link.d);
	*out.d <<= *link.d;

	return out;
}
gtry::scl::TileLinkUB ul2ub(gtry::scl::TileLinkUL& link)
{
	scl::TileLinkUB out;

	out.a = constructFrom(link.a);
	link.a <<= out.a;

	*out.d = constructFrom(*link.d);
	*out.d <<= *link.d;

	return out;
}

SimProcess simuAvalonFakeMemory(const AvalonMM& avmm, const Clock& clk, const size_t memSize, size_t memLatency = 0) {
	std::vector<uint32_t> mem(memSize);
	
	while (true) {
		simu(avmm.ready.value()) = '1';
		simu(avmm.readData.value()).invalidate();
		simu(avmm.readDataValid.value()) = '0';


		while (simu(avmm.read.value()) == '0' && simu(avmm.write.value()) == '0') {
			co_await OnClk(clk);
		}
		HCL_ASSERT((simu(avmm.read.value()) == '1' && simu(avmm.write.value()) == '1') != true);

		simu(avmm.ready.value()) = '0';
		size_t address = simu(avmm.address);
		bool isRead = simu(avmm.read.value()) == '1';
		bool isWrite = simu(avmm.write.value()) == '1';
		uint32_t dataToWrite = simu(avmm.writeData.value());

		for (size_t i = 0; i < memLatency; i++)
		{
			co_await OnClk(clk);
		}
		if (isRead) {
			simu(avmm.readData.value()) = mem[address];
			simu(avmm.readDataValid.value()) = '1';
		}
		else if (isWrite) {
			mem[address] = dataToWrite;
		}
		simu(avmm.ready.value()) = '1';
		co_await OnClk(clk);
	}
}

BOOST_FIXTURE_TEST_CASE(tl_to_amm_basic_test, BoostUnitTestSimulationFixture) {
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	AvalonMM avmm;
	avmm.read.emplace();
	avmm.readDataValid.emplace();
	avmm.ready.emplace();
	avmm.write.emplace();
	avmm.address = 8_b;
	avmm.byteEnable = 2_b;
	avmm.writeData = 16_b;
	avmm.readData = 16_b;

	avmm.pinOut("avmm_");
	
	scl::TileLinkUL in = makeTlSlave(avmm, 4_b);

	//pinIn(in, "in_");
	//avmm.readLatency = 1;
	attachMem(avmm, 8_b);
	
	scl::TileLinkMasterModel linkModel;
	linkModel.init("tlmm_", 8_b, 16_b, 1_b, 4_b);
	ul2ub(in) <<= linkModel.getLink();

	addSimulationProcess([&]()->SimProcess {
		//fork(simuAvalonFakeMemory(avmm, clock, avmm.address.width().count()));
		
		co_await OnClk(clock);
		fork(linkModel.put(0x00, 1, 0xAAAA, clock));
		
		for (size_t i = 0; i < 3; i++)
			co_await OnClk(clock);

		{
			auto [val, def, err] = co_await linkModel.get(0x00, 1, clock);
			BOOST_TEST(!err);
			BOOST_TEST((val & def) == 0xAAAA);
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}
