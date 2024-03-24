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
#include <gatery/scl/stream/SimuHelpers.h>

#include <gatery/export/DotExport.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;


BOOST_FIXTURE_TEST_CASE(tileLink_requester_test_read_1word, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth tlpW = 256_b;

	const size_t memSizeInBytes = 4;
	static_assert(memSizeInBytes % 4 == 0);

	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel model(testSpace);
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
	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tileLink_requester_test_read_any_word_in_256_b_data_beat, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth tlpW = 512_b;

	const size_t memSizeInBytes = 32;
	static_assert(memSizeInBytes % 4 == 0);

	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel model(testSpace);

	model.defaultHandlers();
	scl::pci::RequesterInterface reqInt = model.requesterInterface(tlpW);
	addSimulationProcess([&, this]()->SimProcess { return model.completeRequests(clk, 3); });

	TileLinkUL slaveTl = scl::pci::makePciMaster(move(reqInt), 8_b, 256_b, 8_b);

	TileLinkMasterModel tlmm;
	tlmm.init("tlmm", 8_b, 256_b, 3_b, 8_b);
	auto& masterTl = (TileLinkUL&) tlmm.getLink();

	slaveTl <<= masterTl;

	//send tilelink requests process ( tilelink master model does not support 256 bit bus)
	addSimulationProcess([&, this]()->SimProcess {
		simu(masterTl.a->param) = 0;
		co_await OnClk(clk);
		//32bit requests
		for (size_t offset = 0; offset < 8; offset++)
		{
			simu(valid(masterTl.a)) = '1';
			simu(masterTl.a->opcode) = (size_t) TileLinkA::OpCode::Get;
			simu(masterTl.a->source) = offset;
			simu(masterTl.a->address) = offset*(32 >> 3);
			simu(masterTl.a->size) = 2;
			simu(masterTl.a->mask) = 0xFull << (offset * 4);
			co_await scl::strm::performTransferWait(masterTl.a, clk);
			simu(valid(masterTl.a)) = '0';
		}
		co_await OnClk(clk);
		//64bit requests
		for (size_t offset = 0; offset < 4; offset++)
		{
			simu(valid(masterTl.a)) = '1';
			simu(masterTl.a->opcode) = (size_t) TileLinkA::OpCode::Get;
			simu(masterTl.a->source) = offset;
			simu(masterTl.a->address) = offset*(64 >> 3);
			simu(masterTl.a->size) = 3;
			simu(masterTl.a->mask) = 0xFFull << (offset * 8);
			co_await scl::strm::performTransferWait(masterTl.a, clk);
			simu(valid(masterTl.a)) = '0';
		}
		co_await OnClk(clk);
		//128 bit requests
		for (size_t offset = 0; offset < 2; offset++)
		{
			simu(valid(masterTl.a)) = '1';
			simu(masterTl.a->opcode) = (size_t) TileLinkA::OpCode::Get;
			simu(masterTl.a->source) = offset;
			simu(masterTl.a->address) = offset*(128 >> 3);
			simu(masterTl.a->size) = 4;
			simu(masterTl.a->mask) = 0xFFFFull << (offset * 16);
			co_await scl::strm::performTransferWait(masterTl.a, clk);
			simu(valid(masterTl.a)) = '0';
		}
		co_await OnClk(clk);
		//256 bit requests
		for (size_t offset = 0; offset < 1; offset++)
		{
			simu(valid(masterTl.a)) = '1';
			simu(masterTl.a->opcode) = (size_t) TileLinkA::OpCode::Get;
			simu(masterTl.a->source) = offset;
			simu(masterTl.a->address) = 0;
			simu(masterTl.a->size) = 5;
			simu(masterTl.a->mask) = 0xFFFFFFFFull;
			co_await scl::strm::performTransferWait(masterTl.a, clk);
			simu(valid(masterTl.a)) = '0';
		}

		}
	);

	//receive and check data
	addSimulationProcess([&, this]()->SimProcess {
		//32 bit check
		for (size_t offset = 0; offset < 8; offset++)
		{
			co_await scl::strm::performTransferWait(*masterTl.d, clk);
			BOOST_TEST(simu((*masterTl.d)->opcode) == (size_t)TileLinkD::OpCode::AccessAckData);
			BOOST_TEST(simu((*masterTl.d)->size) == 2);
			BOOST_TEST(simu((*masterTl.d)->source) == offset);
			auto dataDBV = (gtry::sim::DefaultBitVectorState) simu((*masterTl.d)->data);
			auto extractedData = dataDBV.extract(offset * 32, 32);
			BOOST_TEST(extractedData == model.memory().read(offset*32, 32));
			BOOST_TEST(simu((*masterTl.d)->error) == '0');
		}

		//64 bit check
		for (size_t offset = 0; offset < 4; offset++)
		{
			co_await scl::strm::performTransferWait(*masterTl.d, clk);
			BOOST_TEST(simu((*masterTl.d)->opcode) == (size_t)TileLinkD::OpCode::AccessAckData);
			BOOST_TEST(simu((*masterTl.d)->size) == 3);
			BOOST_TEST(simu((*masterTl.d)->source) == offset);
			auto dataDBV = (gtry::sim::DefaultBitVectorState) simu((*masterTl.d)->data);
			auto extractedData = dataDBV.extract(offset * 64, 64);
			BOOST_TEST(extractedData == model.memory().read(offset*64, 64));
			BOOST_TEST(simu((*masterTl.d)->error) == '0');
		}

		//128 bit check
		for (size_t offset = 0; offset < 2; offset++)
		{
			co_await scl::strm::performTransferWait(*masterTl.d, clk);
			BOOST_TEST(simu((*masterTl.d)->opcode) == (size_t)TileLinkD::OpCode::AccessAckData);
			BOOST_TEST(simu((*masterTl.d)->size) == 4);
			BOOST_TEST(simu((*masterTl.d)->source) == offset);
			auto dataDBV = (gtry::sim::DefaultBitVectorState) simu((*masterTl.d)->data);
			auto extractedData = dataDBV.extract(offset * 128, 128);
			BOOST_TEST(extractedData == model.memory().read(offset*128, 128));
			BOOST_TEST(simu((*masterTl.d)->error) == '0');
		}

		//256 bit check
		for (size_t offset = 0; offset < 1; offset++)
		{
			co_await scl::strm::performTransferWait(*masterTl.d, clk);
			BOOST_TEST(simu((*masterTl.d)->opcode) == (size_t)TileLinkD::OpCode::AccessAckData);
			BOOST_TEST(simu((*masterTl.d)->size) == 5);
			BOOST_TEST(simu((*masterTl.d)->source) == offset);
			auto dataDBV = (gtry::sim::DefaultBitVectorState) simu((*masterTl.d)->data);
			auto extractedData = dataDBV.extract(offset * 256, 256);
			BOOST_TEST(extractedData == model.memory().read(offset*256, 256));
			BOOST_TEST(simu((*masterTl.d)->error) == '0');
		}
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});
	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

