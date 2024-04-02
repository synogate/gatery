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
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/tilelink/TileLinkValidator.h>
#include <gatery/scl/Avalon.h>
#include <gatery/scl/tileLinkBridge.h>
#include <gatery/scl/stream/Stream.h>

#include <gatery/utils/Range.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

static gtry::scl::TileLinkUB ul2ub(gtry::scl::TileLinkUL& link)
{
	scl::TileLinkUB out;

	out.a = constructFrom(link.a);
	link.a <<= out.a;

	*out.d = constructFrom(*link.d);
	*out.d <<= *link.d;

	return out;
}

SimProcess simuTileLinkMemCoherenceSupervisor(const TileLinkUL& tl, const Clock& clk) {
	using addr_t = uint64_t;
	using source_t = uint64_t;
	std::map<addr_t, sim::DefaultBitVectorState> mem;

	struct GetRequest {
		uint64_t address;
		sim::DefaultBitVectorState dataInMem;
	};

	std::map<source_t, GetRequest> getRequestsPending;

	fork([&]()->SimProcess {
		while (true) {
			co_await performTransferWait((*tl.d), clk);
			if (simu((*tl.d)->opcode) == (size_t) TileLinkD::AccessAckData) {
				auto it = getRequestsPending.find(simu((*tl.d)->source));
				bool checkThatSourceIsNew = it != getRequestsPending.end();
				BOOST_TEST(checkThatSourceIsNew);
				if (checkThatSourceIsNew) {
					auto& memWord = it->second.dataInMem;
					if (memWord.size() == 0)
						memWord.resize(tl.a->data.width().value);

					bool foundAddress = mem.find(it->second.address) != mem.end();
					BOOST_TEST(foundAddress);
					BOOST_TEST(memWord == simu((*tl.d)->data));

					getRequestsPending.erase(it);
				}
			}
		}
	});

	while(true){
		co_await performTransferWait(tl.a, clk);

		BOOST_TEST(simu(tl.a->opcode).allDefined());
		auto truncation = gtry::utils::Log2C(tl.a->data.width().bytes());

		if (simu(tl.a->opcode) == (size_t) TileLinkA::Get) {
			GetRequest getRequest;

			BOOST_TEST(simu(tl.a->address).allDefined());

			getRequest.address = simu(tl.a->address) >> truncation;
			getRequest.dataInMem = mem[getRequest.address];

			bool check = getRequestsPending.find(simu(tl.a->source)) == getRequestsPending.end();
			BOOST_TEST(check);
			getRequestsPending[simu(tl.a->source)] = getRequest;
		}
		else if (simu(tl.a->opcode) == (size_t)TileLinkA::PutFullData || simu(tl.a->opcode) == (size_t)TileLinkA::PutPartialData) {
			uint64_t address = simu(tl.a->address) >> truncation;

			auto& memWord = mem[address];

			if (memWord.size() == 0)
				memWord.resize(tl.a->data.width().value);

			const sim::DefaultBitVectorState& wrMask = simu(tl.a->mask);
			const sim::DefaultBitVectorState wrData = simu(tl.a->data);

			for (auto byteIdx : gtry::utils::Range(wrMask.size())) {

				size_t byteToWrite_value = memWord.extract(sim::DefaultConfig::VALUE, byteIdx * 8, 8);
				size_t byteToWrite_defined = memWord.extract(sim::DefaultConfig::DEFINED, byteIdx * 8, 8);

				size_t newByteToWrite_value = wrData.extract(sim::DefaultConfig::VALUE, byteIdx * 8, 8);
				size_t newByteToWrite_defined = wrData.extract(sim::DefaultConfig::DEFINED, byteIdx * 8, 8);

				if (wrMask.get(sim::DefaultConfig::DEFINED, byteIdx)) {
					if (wrMask.get(sim::DefaultConfig::VALUE, byteIdx)) {
						byteToWrite_defined = newByteToWrite_defined;
						byteToWrite_value = newByteToWrite_value;
					}
				}
				else {
					byteToWrite_defined = byteToWrite_defined & newByteToWrite_defined & (byteToWrite_value ^ newByteToWrite_value);
					byteToWrite_value = newByteToWrite_value;
				}

				memWord.insert(sim::DefaultConfig::VALUE, byteIdx * 8, 8, byteToWrite_value);
				memWord.insert(sim::DefaultConfig::DEFINED, byteIdx * 8, 8, byteToWrite_defined);
			}
		}
	}
}

struct TranslatorTextSimulationFixture : public BoostUnitTestSimulationFixture {
public:
	void prepareTest(scl::TileLinkUL& in, AvalonMM& avmm, scl::TileLinkMasterModel& linkModel, const Clock& clock) {

		avmm.read.emplace();
		avmm.readDataValid.emplace();
		avmm.ready.emplace();
		avmm.write.emplace();
		avmm.address = 4_b;
		avmm.byteEnable = 2_b;
		avmm.writeData = 16_b;
		avmm.readData = avmm.writeData->width();
		avmm.maximumPendingReadTransactions = 32;
		avmm.maximumPendingWriteTransactions = 32;

		in = tileLinkBridge(avmm, 4_b);

		avmm.readLatency = 5;
		attachMem(avmm, avmm.address.width());

		std::string pinName = "avmm_";
		gtry::pinOut(avmm.address).setName(pinName + "address");
		if (avmm.read) gtry::pinOut(*avmm.read).setName(pinName + "read");
		if (avmm.write) gtry::pinOut(*avmm.write).setName(pinName + "write");
		if (avmm.writeData) gtry::pinOut(*avmm.writeData).setName(pinName + "writedata");
		if (avmm.byteEnable) gtry::pinOut(*avmm.byteEnable).setName(pinName + "byteenable");
		if (avmm.ready) gtry::pinOut(*avmm.ready).setName(pinName + "waitrequest_n");
		if (avmm.readData) gtry::pinOut(*avmm.readData).setName(pinName + "readdata");
		if (avmm.readDataValid) gtry::pinOut(*avmm.readDataValid).setName(pinName + "readdatavalid");

		linkModel.init("tlmm_", 
			in.a->address.width(),
			in.a->data.width(),
			in.a->size.width(),
			in.a->source.width());

		ul2ub(in) <<= linkModel.getLink();

		addSimulationProcess([&]()->SimProcess {
			co_await simuTileLinkMemCoherenceSupervisor(in, clock);
		});

		addSimulationProcess([&]()->SimProcess {
			co_await OnClk(clock);
			co_await validateTileLink(in.a, (*in.d), clock);
		});

	}
};

BOOST_FIXTURE_TEST_CASE(tl_to_amm_basic_test, TranslatorTextSimulationFixture) {

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::TileLinkMasterModel linkModel;

	scl::TileLinkUL in;
	AvalonMM avmm;

	prepareTest(in, avmm, linkModel, clock);

	addSimulationProcess([&]()->SimProcess {

		co_await OnClk(clock);
		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.put(i*2, 1, i, clock));
		}

		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.get(i*2, 1, clock));
		}
		co_await linkModel.get(10, 1, clock);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tl_to_amm_basic_test_chaos_monkey, TranslatorTextSimulationFixture) {

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::TileLinkMasterModel linkModel;
	linkModel.probability(0.5f, 0.5f); //start rv chaos monkey
	scl::TileLinkUL in;
	AvalonMM avmm;

	prepareTest(in, avmm, linkModel, clock);

	addSimulationProcess([&]()->SimProcess {

		co_await OnClk(clock);

		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.put(i*2, 1, i, clock));
		}

		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.get(i*2, 1, clock));
		}
		co_await linkModel.get(10, 1, clock);

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tl_to_amm_partial_basic_test, TranslatorTextSimulationFixture) {

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::TileLinkMasterModel linkModel;

	scl::TileLinkUL in;
	AvalonMM avmm;

	prepareTest(in, avmm, linkModel, clock);

	addSimulationProcess([&]()->SimProcess {

		co_await OnClk(clock);
		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.put(i, 0, i, clock));
		}

		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.get(i, 0, clock));
		}
		co_await linkModel.get(10, 1, clock);

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tl_to_amm_put_get, TranslatorTextSimulationFixture) {

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::TileLinkMasterModel linkModel;

	scl::TileLinkUL in;
	AvalonMM avmm;

	prepareTest(in, avmm, linkModel, clock);

	addSimulationProcess([&]()->SimProcess {

		co_await OnClk(clock);
		for (size_t i = 0; i < 10; i++)
		{
			fork(linkModel.put(0x4, 0, i, clock));
			fork(linkModel.get(0x4, 0, clock));
		}
		co_await linkModel.get(10, 1, clock);

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(tl_to_amm_fuzzing, TranslatorTextSimulationFixture) {
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::TileLinkMasterModel linkModel;
	scl::TileLinkUL in;
	AvalonMM avmm;
	linkModel.probability(0.5f, 0.5f); //start rv chaos monkey
	prepareTest(in, avmm, linkModel, clock);

	addSimulationProcess([&]()->SimProcess {
		std::mt19937 gen(2182818284);
		std::uniform_int_distribution<uint64_t> dist;

		co_await OnClk(clock);
		for (size_t i = 0; i < 256; i++)
		{
			size_t size = dist(gen) & 0x1;
			uint64_t address = dist(gen) & ~size;

			if (dist(gen) & 0x1)
				fork(linkModel.put(address, size, dist(gen), clock));
			else if (dist(gen) & 0x1)
				fork(linkModel.get(address, size, clock));

			co_await OnClk(clock);
		}
		co_await linkModel.get(0, 1, clock);
		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}
