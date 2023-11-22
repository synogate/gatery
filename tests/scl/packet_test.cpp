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

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/Simulator.h>
#include <gatery/simulation/BitVectorState.h>

#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/utils.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/stream/FieldExtractor.h>
#include <gatery/scl/io/SpiMaster.h> 

#include <gatery/debug/websocks/WebSocksInterface.h>
#include <gatery/scl/sim/SimulationSequencer.h>


using namespace boost::unit_test;
using namespace gtry;


template<typename StreamType>
struct PacketSendAndReceiveTest : public BoostUnitTestSimulationFixture
{
	Clock packetTestclk = Clock({ .absoluteFrequency = 100'000'000 });
	std::vector<scl::strm::SimPacket> allPackets;
	bool addPipelineReg = true;
	BitWidth txIdW = 4_b;
	bool backpressureRNG = false;
	size_t readyProbabilityPercent = 50;
	std::uint64_t unreadyMask = 0;

	void runTest() {
		ClockScope clkScp(packetTestclk);

		StreamType in = { 16_b };
		StreamType out = { 16_b };

		if constexpr (StreamType::template has<scl::Empty>()) {
			empty(in) = BitWidth::last(in->width().bytes() - 1);
			empty(out) = BitWidth::last(in->width().bytes() - 1);
		}
		if constexpr (StreamType::template has<scl::TxId>()) {
			txid(in) = txIdW;
			txid(out) = txIdW;
		}

		if (addPipelineReg)
			out <<= scl::strm::regDownstream(move(in));
		else
			out <<= in;

		pinIn(in, "in_");
		pinOut(out, "out_");


		addSimulationProcess([&, this]()->SimProcess {
			scl::SimulationSequencer sendingSequencer;
			
			if constexpr (StreamType::template has<scl::Ready>())
				if(backpressureRNG)
					fork(readyDriverRNG(out, packetTestclk, readyProbabilityPercent));
				else
					fork(readyDriver(out, packetTestclk, unreadyMask));

			for (const auto& packet : allPackets)
				fork(sendPacket(in, packet, packetTestclk, sendingSequencer));

			for (const auto& packet : allPackets) {
				scl::strm::SimPacket rvdPacket = co_await receivePacket(out, packetTestclk);
				BOOST_TEST(rvdPacket.payload == packet.payload);

				if constexpr (StreamType::template has<scl::TxId>()) {
					BOOST_TEST(rvdPacket.txid() == packet.txid());
				}
				if constexpr (StreamType::template has<scl::Error>()) {
					BOOST_TEST(rvdPacket.error() == packet.error());
				}
			}
			stopTest();
			});

		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
	}
};


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_singleBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<gtry::BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_multiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_longMultiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_packetStream, PacketSendAndReceiveTest<scl::PacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	addPipelineReg = false;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RvPacketStream, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_VPacketStream, PacketSendAndReceiveTest<scl::VPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream, PacketSendAndReceiveTest<scl::RsPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_SPacketStream, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RvPacketStream_bubbles, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RvPacketStream_bubbles_backpressure, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	unreadyMask = 0b10110001101;
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}
BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RvPacketStream_rng_backpressure, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	backpressureRNG = true;
	readyProbabilityPercent = 70;
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

using RsePacketStream = scl::RsPacketStream<BVec, scl::Empty>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty, PacketSendAndReceiveTest<RsePacketStream>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }),
	};
	runTest();
}


using RseePacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::Error>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_error, PacketSendAndReceiveTest<RseePacketStream>) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).error('0'),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).error('1'),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).error('0'),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).error('1'),
	};
	runTest();
}

using RsetPacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::TxId>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_txid, PacketSendAndReceiveTest<RsetPacketStream>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).txid(0),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).txid(1),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).txid(2),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).txid(0),
	};
	runTest();
}

using RsePacketStream = scl::RsPacketStream<BVec, scl::Empty>;
BOOST_FIXTURE_TEST_CASE(packetSenderFramework_test_RvStream, PacketSendAndReceiveTest<RsePacketStream>) {
	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket{0xABCD , 16_b},
		scl::strm::SimPacket{0xABCD , 32_b},
		scl::strm::SimPacket{0xABCD , 48_b},
		scl::strm::SimPacket{0xABCD , 64_b},
		scl::strm::SimPacket{0xABCD , 80_b},
		scl::strm::SimPacket{0xABCD , 128_b},
	};

	runTest();
}



template<typename StreamType, typename OutStreamType>
struct FieldExtractionTest : public BoostUnitTestSimulationFixture
{
	Clock packetTestclk = Clock({ .absoluteFrequency = 100'000'000 });
	BitWidth streamWidth = 16_b;
	std::vector<scl::strm::SimPacket> allPackets;
	//bool addPipelineReg = true;
	BitWidth txIdW = 0_b;
	bool backpressureRNG = false;
	size_t readyProbabilityPercent = 50;

	std::vector<scl::Field> fields;
	std::mt19937 gen;

	FieldExtractionTest() : gen(23456789) { }
						

	void randomPackets(size_t count, size_t minSize, size_t maxSize) {
		allPackets.resize(count);
		for (auto &packet : allPackets) {
			std::uniform_int_distribution<size_t> sizeDist(minSize, maxSize + 1);
			size_t size = (sizeDist(gen)*8 + streamWidth.value - 1) / streamWidth.value * streamWidth.value;

			std::vector<uint8_t> payload;
			payload.resize(size / 8);
			for (auto &b : payload)
				b = gen();

			packet = payload;

			if (txIdW.value > 0) packet.txid(gen() % txIdW.count());
		}
	}

	void randomFields(size_t count, size_t start, size_t end) {
		fields.resize(count);
		for (auto &field : fields) {
			std::uniform_int_distribution<size_t> offsetDist(start, end-1);
			field.offset = offsetDist(gen);

			std::uniform_int_distribution<size_t> sizeDist(1, end - field.offset);
			field.size = BitWidth(sizeDist(gen));
		}
	}	

	void runTest() {
		ClockScope clkScp(packetTestclk);

		StreamType in = { streamWidth };
		if constexpr (StreamType::template has<scl::Empty>()) {
			empty(in) = 1_b;
		}
		if constexpr (StreamType::template has<scl::TxId>()) {
			txid(in) = txIdW;
		}
		OutStreamType out;
		scl::extractFields(out, in, fields);

		pinIn(in, "in_");
		pinOut(out, "out_");


		addSimulationProcess([&, this]()->SimProcess {
		
			if constexpr (StreamType::template has<scl::Ready>())
				if(backpressureRNG)
					fork([&,this]()->SimProcess {
						std::mt19937 gen(1234567890);
						std::uniform_int_distribution<size_t> distrib(0,99);
						while (true) {
							simu(ready(out)) = distrib(gen) < readyProbabilityPercent;
							co_await OnClk(packetTestclk);
						}
					});
				else
					simu(ready(out)) = '1';
					
			

			fork([&,this]()->SimProcess {
				for (const auto& packet : allPackets)
					co_await sendPacket(in, packet, packetTestclk);
			});

			size_t packetIdx = 0;
			for (const auto& packet : allPackets) {

				co_await performTransferWait(out, packetTestclk);

				if constexpr (StreamType::template has<scl::TxId>()) {
					BOOST_TEST(simu(txid(out)) == packet.txid());
				}

				bool packetTooShort = false;
				for (const auto &f : fields)
					if (f.offset + f.size.value > packet.payload.size()) {
						packetTooShort = true;
						break;
					}

				BOOST_TEST(simu(error(out)) == packetTooShort);

				if (!packetTooShort)
					for (auto fieldIdx : gtry::utils::Range(out->size())) {
						auto expected = packet.payload.extract(fields[fieldIdx].offset, fields[fieldIdx].size.value);
						BOOST_TEST(simu(out->at(fieldIdx)) == expected,
							"Error in field " << fieldIdx << " for packet " << packetIdx << ": Circuit yielded " << simu(out->at(fieldIdx)) << " but should be " << expected);
					}

				packetIdx++;
			}
			stopTest();
		});

		//design.visualize("before");
		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 150, 1'000'000 }));
	}
};


using VFieldStream = scl::VStream<gtry::Vector<BVec>, scl::Error>;
using FieldExtractionTest_VFieldStream = FieldExtractionTest<scl::VPacketStream<BVec>, VFieldStream>;

BOOST_FIXTURE_TEST_CASE(fieldExtraction, FieldExtractionTest_VFieldStream) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, }).invalidBeats(gen()),
	};


	for (size_t offset : {0, 8, 16, 24})
		for (size_t size : {4, 8, 16, 24})
			fields.push_back(scl::Field {
				.offset = offset,
				.size = BitWidth(size),
			});


	runTest();
}

BOOST_FIXTURE_TEST_CASE(fieldExtractionSingleBeat, FieldExtractionTest_VFieldStream) {

	BOOST_REQUIRE(streamWidth == 16_b);

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21 }),
	};


	for (size_t offset : {0, 1, 2, 4})
		for (size_t size : {1, 2, 4})
			fields.push_back(scl::Field {
				.offset = offset,
				.size = BitWidth(size),
			});


	runTest();
}

BOOST_FIXTURE_TEST_CASE(fieldExtractionPacketsTooShort, FieldExtractionTest_VFieldStream) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21 }).invalidBeats(gen()),
	};


	for (size_t offset : {0, 8, 16, 24})
		for (size_t size : {4, 8, 16, 24})
			fields.push_back(scl::Field {
				.offset = offset,
				.size = BitWidth(size),
			});


	runTest();
}

using FieldExtractionTest_VFieldStream_Empty = FieldExtractionTest<scl::VPacketStream<BVec, scl::Empty>, VFieldStream>;

BOOST_FIXTURE_TEST_CASE(fieldExtractionPacketsTooShortEmpty, FieldExtractionTest_VFieldStream_Empty) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11, 0x12 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21 }).invalidBeats(gen()),
	};

	fields.push_back(scl::Field {
		.offset = 16,
		.size = 10_b,
	});

	runTest();
}


BOOST_FIXTURE_TEST_CASE(fieldExtractionPacketsNotTooShortEmpty, FieldExtractionTest_VFieldStream_Empty) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11, 0x12 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21 }).invalidBeats(gen()),
	};

	fields.push_back(scl::Field {
		.offset = 16,
		.size = 7_b,
	});

	runTest();
}


BOOST_FIXTURE_TEST_CASE(fieldExtractionFuzz, FieldExtractionTest_VFieldStream) {

	randomPackets(100, 20, 50);
	randomFields(10, 0, 20);

	runTest();
}


using RvFieldStream = scl::RvStream<gtry::Vector<BVec>, scl::Error>;
using FieldExtractionTest_RvFieldStream = FieldExtractionTest<scl::RvPacketStream<BVec>, RvFieldStream>;


BOOST_FIXTURE_TEST_CASE(fieldExtractionWBackPressure, FieldExtractionTest_RvFieldStream) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, }).invalidBeats(gen()),
	};


	for (size_t offset : {0, 8, 16, 24})
		for (size_t size : {4, 8, 16, 24})
			fields.push_back(scl::Field {
				.offset = offset,
				.size = BitWidth(size),
			});

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest();
}


BOOST_FIXTURE_TEST_CASE(fieldExtractionWHighBackPressure, FieldExtractionTest_RvFieldStream) {

	allPackets = std::vector<scl::strm::SimPacket>{
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19 }).invalidBeats(gen()),
		scl::strm::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, }).invalidBeats(gen()),
	};


	for (size_t offset : {0, 8, 16, 24})
		for (size_t size : {4, 8, 16, 24})
			fields.push_back(scl::Field {
				.offset = offset,
				.size = BitWidth(size),
			});

	backpressureRNG = true;
	readyProbabilityPercent = 5;


	runTest();
}



BOOST_FIXTURE_TEST_CASE(fieldExtractionFuzzWBackPressure, FieldExtractionTest_RvFieldStream) {

	randomPackets(100, 20, 50);
	randomFields(10, 0, 20);

	backpressureRNG = true;
	readyProbabilityPercent = 80;

	runTest();
}


using RvTxidFieldStream = scl::RvStream<gtry::Vector<BVec>, scl::TxId, scl::Error>;
using FieldExtractionTest_RvTxidFieldStream = FieldExtractionTest<scl::RsPacketStream<BVec, scl::Empty, scl::TxId>, RvTxidFieldStream>;

BOOST_FIXTURE_TEST_CASE(fieldExtractionFuzz_RsetPacketStream, FieldExtractionTest_RvTxidFieldStream) {

	txIdW = 4_b;

	randomPackets(100, 20, 50);
	randomFields(10, 0, 20);

	backpressureRNG = true;
	readyProbabilityPercent = 80;

	runTest();
}


BOOST_FIXTURE_TEST_CASE(fieldExtractionFuzz_RsetPacketStreamWHighBackPressure, FieldExtractionTest_RvTxidFieldStream) {

	txIdW = 4_b;

	randomPackets(100, 20, 50);
	randomFields(10, 0, 20);

	backpressureRNG = true;
	readyProbabilityPercent = 2;

	runTest();
}
