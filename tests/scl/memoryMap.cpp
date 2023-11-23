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
#include <iostream>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::utils;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;


struct MyStruct {
	BOOST_HANA_DEFINE_STRUCT(MyStruct,
		(Bit, write),
		(BVec, address),
		(BVec, data)
	);
};

template<>
struct gtry::CompoundAnnotator<MyStruct> {
	static CompoundAnnotation annotation;
};

CompoundAnnotation gtry::CompoundAnnotator<MyStruct>::annotation = {
		.shortDesc = "Bla",
		.longDesc = "muchBla",
		.memberDesc = {
			CompoundMemberAnnotation{ .shortDesc = "writeBla" },
			CompoundMemberAnnotation{ .shortDesc = "addressBla" },
			CompoundMemberAnnotation{ .shortDesc = "dataBla" },
		}
	};




struct IdiotBus {
	BOOST_HANA_DEFINE_STRUCT(IdiotBus,
		(UInt, address),
		(gtry::Reverse<BVec>, rdData),
		(BVec, wrData),
		(Bit, write)
	);	
};


BOOST_AUTO_TEST_SUITE(MemoryMap);

BOOST_FIXTURE_TEST_CASE(MemoryMapStruct, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	{
		MyStruct myStruct{ Bit{}, 4_b, 16_b };

		pinOut(myStruct, "myStruct");

		PackedMemoryMap memoryMap("myMemoryMap");
		mapIn(memoryMap, myStruct, "myStruct");
		mapOut(memoryMap, myStruct, "myStruct");

		memoryMap.packRegisters(8_b);

		auto &tree = memoryMap.getTree();
		std::cout << tree.physicalDescription << std::endl;

		//BOOST_TEST(tree.name == "myMemoryMap");
		//BOOST_TEST(tree.subScopes.front().name == "myStream");
		//BOOST_TEST(tree.subScopes.front().registeredSignals.front().name == "payload");
		//BOOST_TEST(tree.subScopes.front().physicalRegisters.front().description.name == "payload");

		IdiotBus bus = {
			.address = 16_b,
			.rdData = 8_b,
			.wrData = 8_b,
		};
		pinIn(bus, "cpuBus");

		std::function<void(PackedMemoryMap::Scope &scope)> processRegs;
		processRegs = [&](PackedMemoryMap::Scope &scope) {
			for (auto &r : scope.physicalRegisters) {
				IF (bus.address == r.description.offsetInBits / bus.rdData->size()) {
					if (r.readSignal)
						*bus.rdData = zext(*r.readSignal);
					
					if (r.writeSignal) {
						IF (bus.write) {
							r.writeSignal = bus.wrData(0, r.writeSignal->width());
							*r.onWrite = '1';
						}
					}
				}
			}
			for (auto &c : scope.subScopes)
				processRegs(c);
		};
		*bus.rdData = dontCare(*bus.rdData);
		processRegs(tree);
	}

	design.visualize("test");

	design.postprocess();

	design.visualize("test_after");

}

BOOST_FIXTURE_TEST_CASE(MemoryMapStream, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::scl;
	using namespace gtry::scl::strm;


	RvStream<BVec> myStream{ 4_b };

	pinOut(myStream, "myStream");

	PackedMemoryMap memoryMap("myMemoryMap");
	mapIn(memoryMap, myStream, "myStream");

	memoryMap.packRegisters(8_b);

	auto &tree = memoryMap.getTree();
	std::cout << tree.physicalDescription << std::endl;

	BOOST_TEST(tree.name == "myMemoryMap");
	BOOST_TEST(tree.subScopes.front().name == "myStream");
	BOOST_TEST(tree.subScopes.front().registeredSignals.front().name == "payload");
	BOOST_TEST(tree.subScopes.front().physicalRegisters.front().description.name == "payload_bits_0_to_8");


	IdiotBus bus = {
		.address = 16_b,
		.rdData = 8_b,
		.wrData = 8_b,
	};
	pinIn(bus, "cpuBus");

	std::function<void(PackedMemoryMap::Scope &scope)> processRegs;
	processRegs = [&](PackedMemoryMap::Scope &scope) {
		for (auto &r : scope.physicalRegisters) {
			IF (bus.address == r.description.offsetInBits / bus.rdData->size()) {
				if (r.readSignal)
					*bus.rdData = zext(*r.readSignal);
				
				if (r.writeSignal) {
					IF (bus.write) {
						r.writeSignal = bus.wrData(0, r.writeSignal->width());
						*r.onWrite = '1';
					}
				}
			}
		}
		for (auto &c : scope.subScopes)
			processRegs(c);
	};
	*bus.rdData = dontCare(*bus.rdData);
	processRegs(tree);

	design.visualize("test");

	design.postprocess();

	design.visualize("test_after");

}


BOOST_AUTO_TEST_SUITE_END();