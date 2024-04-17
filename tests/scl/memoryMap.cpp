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

#include <gatery/simulation/Simulator.h>
#include <gatery/scl/stream/Stream.h>
#include <gatery/scl/memoryMap/PackedMemoryMap.h>
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>
#include <gatery/scl/memoryMap/TileLinkMemoryMap.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <iostream>


using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


struct MyStruct {
	BOOST_HANA_DEFINE_STRUCT(MyStruct,
		(Bit, field1),
		(BVec, field2),
		(BVec, field3)
	);
};

template<>
struct gtry::CompoundAnnotator<MyStruct> {
	static CompoundAnnotation annotation;
};

CompoundAnnotation gtry::CompoundAnnotator<MyStruct>::annotation = {
		.shortDesc = "Short description",
		.longDesc = "Long description",
		.memberDesc = {
			CompoundMemberAnnotation{ .shortDesc = "Desc field 1" },
			CompoundMemberAnnotation{ .shortDesc = "Desc field 2" },
			CompoundMemberAnnotation{ .shortDesc = "Desc field 3" },
		}
	};


BOOST_AUTO_TEST_SUITE(MemoryMap);

BOOST_FIXTURE_TEST_CASE(MemoryMapStruct, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;

	MyStruct myStruct{ Bit{}, 4_b, 16_b };

	pinOut(myStruct, "myStruct");

	PackedMemoryMap memoryMap("myMemoryMap");
	mapIn(memoryMap, myStruct, "myStruct");
	mapOut(memoryMap, myStruct, "myStruct");

	memoryMap.packRegisters(8_b);

	auto &tree = memoryMap.getTree();
	//std::cout << tree.physicalDescription << std::endl;

	BOOST_TEST(tree.name == "myMemoryMap");
	BOOST_TEST(tree.subScopes.front().name == "myStruct");
	BOOST_TEST(tree.subScopes.front().registeredSignals.front().name == "field1");
	BOOST_TEST(tree.subScopes.front().physicalRegisters.front().description->name == "field1");
}

BOOST_FIXTURE_TEST_CASE(MemoryMapStream, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	RvStream<BVec> myStream{ 12_b };

	pinOut(myStream, "myStream");

	PackedMemoryMap memoryMap("myMemoryMap");
	mapIn(memoryMap, myStream, "myStream");

	memoryMap.packRegisters(8_b);

	auto &tree = memoryMap.getTree();
	//std::cout << tree.physicalDescription << std::endl;

	BOOST_TEST(tree.name == "myMemoryMap");
	BOOST_TEST(tree.subScopes.front().name == "myStream");
	BOOST_TEST(tree.subScopes.front().registeredSignals.front().name == "payload");
	BOOST_TEST(tree.subScopes.front().physicalRegisters.front().description->name == "payload_bits_0_to_7");
}



namespace {

gtry::scl::TileLinkUB ul2ub(gtry::scl::TileLinkUL& link)
{
	scl::TileLinkUB out;

	out.a = constructFrom(link.a);
	link.a <<= out.a;

	*out.d = constructFrom(*link.d);
	*out.d <<= *link.d;

	return out;
}

}

BOOST_FIXTURE_TEST_CASE(MemoryMapStructTileLink, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	MyStruct myStruct{ Bit{}, 4_b, 20_b };
	TileLinkUL cpuInterface;
	pinOut(myStruct, "myStruct");
	{

		PackedMemoryMap memoryMap("myMemoryMap");
		mapIn(memoryMap, myStruct, "myStruct");
		mapOut(memoryMap, myStruct, "myStruct");

		
		auto tileLink = toTileLinkUL(memoryMap, 8_b);
		cpuInterface = constructFrom(*tileLink);
		*tileLink <<= cpuInterface;
	}
	

	//scl::format(std::cout, *cpuInterface.addrSpaceDesc);

	scl::TileLinkMasterModel linkModel;
	linkModel.init("cpuBus", 
		cpuInterface.a->address.width(), 
		cpuInterface.a->data.width(), 
		cpuInterface.a->size.width(), 
		cpuInterface.a->source.width());

	ul2ub(cpuInterface) <<= linkModel.getLink();

	addSimulationProcess([&]()->SimProcess {
		
		co_await OnClk(clock);

		co_await linkModel.put(0, 0, 1, clock);
		BOOST_TEST(simu(myStruct.field1) == true);

		co_await linkModel.put(1, 0, 0xBA, clock);
		BOOST_TEST(simu(myStruct.field2) == 0xA);

		co_await linkModel.put(2, 0, 0xBA, clock);
		co_await linkModel.put(3, 0, 0xDC, clock);
		co_await linkModel.put(4, 0, 0xFE, clock);
		BOOST_TEST(simu(myStruct.field3) == 0xEDCBA);


		auto [val, def, err] = co_await linkModel.get(2, 0, clock);
		BOOST_TEST(!err);
		BOOST_TEST((val & def) == 0xBA);

		std::tie(val, def, err) = co_await linkModel.get(5, 0, clock);
		BOOST_TEST(err);

		co_await OnClk(clock);
		stopTest();
	});

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(MemoryMapStructTileLinkTileLinkDemux, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	MyStruct myStruct1{ Bit{}, 4_b, 20_b };
	pinOut(myStruct1, "myStruct1");
	MyStruct myStruct2{ Bit{}, 4_b, 20_b };
	pinOut(myStruct2, "myStruct2");

	TileLinkUL cpuInterface;
	{
		TileLinkUL cpuInterface1;
		TileLinkUL cpuInterface2;
		{

			PackedMemoryMap memoryMap("myMemoryMap1");
			mapIn(memoryMap, myStruct1, "myStruct1");
			mapOut(memoryMap, myStruct1, "myStruct1");

			
			auto tileLink = toTileLinkUL(memoryMap, 8_b);
			cpuInterface1 = constructFrom(*tileLink);
			*tileLink <<= cpuInterface1;
		}

		{

			PackedMemoryMap memoryMap("myMemoryMap2");
			mapIn(memoryMap, myStruct1, "myStruct1");
			mapOut(memoryMap, myStruct1, "myStruct1");

			
			auto tileLink = toTileLinkUL(memoryMap, 8_b);
			cpuInterface2 = constructFrom(*tileLink);
			*tileLink <<= cpuInterface2;
		}

		scl::TileLinkDemux<TileLinkUL> demux;

		cpuInterface = scl::tileLinkInit<scl::TileLinkUL>(16_b, 8_b, 0_b);
		demux.attachSource(cpuInterface);

		demux.attachSink(cpuInterface1, 0x1000);
		demux.attachSink(cpuInterface2, 0x2000);

		demux.generate();
	}

	//scl::format(std::cout, *cpuInterface.addrSpaceDesc);

	auto *desc = cpuInterface.addrSpaceDesc->getNonForwardingElement();

	BOOST_TEST(desc->name == "TileLinkDemux");
	BOOST_REQUIRE(desc->children.size() == 2);
	BOOST_TEST(desc->children[0].offsetInBits == 0x1000*8);
	BOOST_TEST(desc->children[0].desc->getNonForwardingElement()->name == "myMemoryMap1");
	BOOST_TEST(desc->children[1].offsetInBits == 0x2000*8);
	BOOST_TEST(desc->children[1].desc->getNonForwardingElement()->name == "myMemoryMap2");
}




BOOST_FIXTURE_TEST_CASE(MemoryMapStreamOutTileLink, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	RvStream<BVec> myStream{ 4_b };
	TileLinkUL cpuInterface;
	pinOut(myStream, "myStream");
	{


		PackedMemoryMap memoryMap("myMemoryMap");
		mapIn(memoryMap, myStream, "myStream");

		auto tileLink = toTileLinkUL(memoryMap, 8_b);
		cpuInterface = constructFrom(*tileLink);
		*tileLink <<= cpuInterface;

		//std::cout << memoryMap.getTree().physicalDescription << std::endl;
	}
	

	scl::TileLinkMasterModel linkModel;
	linkModel.init("cpuBus", 
		cpuInterface.a->address.width(), 
		cpuInterface.a->data.width(), 
		cpuInterface.a->size.width(), 
		cpuInterface.a->source.width());

	ul2ub(cpuInterface) <<= linkModel.getLink();

	addSimulationProcess([&]()->SimProcess {
		simuReady(myStream) = false;
		
		co_await OnClk(clock);
		BOOST_TEST(simuValid(myStream) == false);

		// set payload to write
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Setting payload");
		co_await linkModel.put(0, 0, 0xAB, clock);

		BOOST_TEST(simuValid(myStream) == false);

		// write valid
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Setting valid");
		co_await linkModel.put(2, 0, 1, clock);

		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting stream valid and payload");
		BOOST_TEST(simuValid(myStream) == true);
		BOOST_TEST(simu(*myStream) == 0xB);

		// Check valid is still holding
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting to see the memory map valid still high");
		auto [val, def, err] = co_await linkModel.get(2, 0, clock);
		BOOST_TEST(val);
		BOOST_TEST(def);
		BOOST_TEST(!err);

		sim::SimulationContext::current()->onDebugMessage(nullptr, "transmit on stream");
		simuReady(myStream) = true;
		co_await AfterClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting stream to become non-valid");
		simuReady(myStream) = false;
		BOOST_TEST(simuValid(myStream) == false);

		co_await AfterClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting memory map valid to drop to low");
		while (true) {
			auto [val, def, err] = co_await linkModel.get(2, 0, clock);
			BOOST_TEST(def);
			BOOST_TEST(!err);
			if (!val) break;
		}

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}





BOOST_FIXTURE_TEST_CASE(MemoryMapStreamInTileLink, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	Clock clock({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock);
	ClockScope scp(clock);

	RvStream<BVec> myStream{ 4_b };
	TileLinkUL cpuInterface;
	pinIn(myStream, "myStream");
	{
		PackedMemoryMap memoryMap("myMemoryMap");
		mapOut(memoryMap, myStream, "myStream");

		auto tileLink = toTileLinkUL(memoryMap, 8_b);
		cpuInterface = constructFrom(*tileLink);
		*tileLink <<= cpuInterface;

		//std::cout << memoryMap.getTree().physicalDescription << std::endl;
	}
	

	scl::TileLinkMasterModel linkModel;
	linkModel.init("cpuBus", 
		cpuInterface.a->address.width(), 
		cpuInterface.a->data.width(), 
		cpuInterface.a->size.width(), 
		cpuInterface.a->source.width());

	ul2ub(cpuInterface) <<= linkModel.getLink();

	addSimulationProcess([&]()->SimProcess {
		simuValid(myStream) = false;
		
		co_await OnClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting stream to not be ready");
		BOOST_TEST(simuReady(myStream) == false);


		co_await OnClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting memory map valid to be low");
		{
			auto [val, def, err] = co_await linkModel.get(1, 0, clock);
			BOOST_TEST(!val);
			BOOST_TEST(def);
			BOOST_TEST(!err);
		}

		fork([&]()->SimProcess {
			co_await OnClk(clock);
			co_await OnClk(clock);
			sim::SimulationContext::current()->onDebugMessage(nullptr, "Offering data on stream");
			simu(*myStream) = 0xA;
			co_await gtry::scl::strm::performTransfer(myStream, clock);
			simu(*myStream).invalidate();
		});

		co_await OnClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting memory map valid to (eventually) become high");
		while (true) {
			auto [val, def, err] = co_await linkModel.get(1, 0, clock);
			BOOST_TEST(def);
			BOOST_TEST(!err);
			if (val) break;
		}

		sim::SimulationContext::current()->onDebugMessage(nullptr, "Reading payload");
		auto [val, def, err] = co_await linkModel.get(0, 0, clock);
		BOOST_TEST(val == 0xA);
		BOOST_TEST(def);
		BOOST_TEST(!err);

		// write ready
		co_await OnClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Setting ready");
		co_await linkModel.put(2, 0, 1, clock);

		co_await OnClk(clock);
		sim::SimulationContext::current()->onDebugMessage(nullptr, "Expecting stream to be non-ready");
		BOOST_TEST(simuReady(myStream) == false);

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}




BOOST_AUTO_TEST_SUITE_END();