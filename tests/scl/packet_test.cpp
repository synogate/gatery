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
#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/utils.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/stream/FieldExtractor.h>

#include <gatery/scl/sim/SimulationSequencer.h>


using namespace boost::unit_test;
using namespace gtry;


template<scl::strm::PacketStreamSignal StreamT>
struct PacketSendAndReceiveTest : public BoostUnitTestSimulationFixture
{
	Clock packetTestclk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp = ClockScope(packetTestclk);
	std::vector<scl::strm::SimPacket> allPackets;

	std::optional<StreamT> in;
	std::optional<StreamT> out;

	bool addPipelineReg = true;
	BitWidth txIdW = 4_b;
	bool backpressureRNG = false;
	size_t readyProbabilityPercent = 50;
	std::uint64_t unreadyMask = 0;

	std::mt19937 gen = std::mt19937{ 23456789 };

	void randomPackets(size_t count, size_t minSize, size_t maxSize) {
		ClockScope clkScp(packetTestclk);
		allPackets.resize(count);
		for (auto &packet : allPackets) {
			std::uniform_int_distribution<size_t> sizeDist(minSize, maxSize);
			if (in) {
				if constexpr (StreamT::template has<scl::Error>()) {
					packet.error((gen() & 0b11) == 0 ? '1' : '0'); // 25% chance of error packet
				}
			}

			std::vector<uint8_t> payload(sizeDist(gen));

			for (auto &b : payload)
				b = gen();

			packet = payload;
			if constexpr (StreamT::template has<scl::TxId>()) {
				if (txIdW.value > 0) packet.txid(gen() % txIdW.count());
			}
		}
	}


	void runTest(bool vcd = false) {
		ClockScope clkScp(packetTestclk);

		if (!in) in.emplace(16_b);
		if (!out) {
			out.emplace((*in)->width());
			if constexpr (StreamT::template has<scl::Empty>()) {
				empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
				empty(*out) = BitWidth::last((*in)->width().bytes() - 1);
			}
			if constexpr (StreamT::template has<scl::TxId>()) {
				txid(*in) = txIdW;
				txid(*out) = txIdW;
			}

			if (addPipelineReg)
				*out <<= scl::strm::regDownstream(move(*in));
			else
				*out <<= *in;
		}

		pinIn(*in, "in_");
		pinOut(*out, "out_");


		addSimulationProcess([&, this]()->SimProcess {
			scl::SimulationSequencer sendingSequencer;
			
			if constexpr (StreamT::template has<scl::Ready>())
				if(backpressureRNG)
					fork(readyDriverRNG(*out, packetTestclk, readyProbabilityPercent));
				else
					fork(readyDriver(*out, packetTestclk, unreadyMask));

			for (const auto& packet : allPackets)
				fork(sendPacket(*in, packet, packetTestclk, sendingSequencer));

			for (const auto& packet : allPackets) {
				scl::strm::SimPacket rvdPacket = co_await receivePacket(*out, packetTestclk);
				BOOST_TEST(rvdPacket.payload == packet.payload);

				if constexpr (StreamT::template has<scl::TxId>()) {
					BOOST_TEST(rvdPacket.txid() == packet.txid());
				}
				if constexpr (StreamT::template has<scl::Error>()) {
					BOOST_TEST(rvdPacket.error() == packet.error());
				}
			}
			stopTest();
			});
		if (vcd) { recordVCD("dut.vcd"); }
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

using RvEmptyPacketStream = scl::RvPacketStream<BVec, scl::Empty>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_test_RvEmpty_non_power_of_2, PacketSendAndReceiveTest<RvEmptyPacketStream>) {

	this->in.emplace(24_b);
	empty(*in) = BitWidth::count((*in)->width().bytes());
	this->out = scl::strm::regDownstream(move(*this->in));
	this->txIdW = 0_b;

	randomPackets(20, 1, 50 );

	runTest();
}


using RvEmptyBitsPacketStream = scl::RvPacketStream<BVec, scl::EmptyBits>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_test_RvEmptyBits_non_power_of_2, PacketSendAndReceiveTest<RvEmptyBitsPacketStream>) {

	this->in.emplace(24_b);
	emptyBits(*in) = BitWidth::count((*in)->width().bits());
	this->out = scl::strm::regDownstream(move(*this->in));
	this->txIdW = 0_b;

	randomPackets(20, 1, 50 );

	runTest();
}

using TestPacketStream = scl::RvPacketStream<BVec, scl::Empty>;
BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_8_32, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(8_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);
	this->txIdW = 0_b;
	
	
	randomPackets(100, 1, 50 );
	
	backpressureRNG = true;
	readyProbabilityPercent = 50;
	
	
	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_32, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(16_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);
	this->txIdW = 0_b;


	randomPackets(100, 1, 50 );

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_8_24, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(8_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthExtend(move(*this->in), 24_b);
	this->txIdW = 0_b;


	randomPackets(100, 1, 50 );

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}


BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_48, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(16_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthExtend(move(*this->in), 48_b);
	this->txIdW = 0_b;


	randomPackets(100, 1, 50 );

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}

using TestPacketStreamWithError = scl::RvPacketStream < BVec, scl::Empty, scl::Error > ;

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_32_with_error, PacketSendAndReceiveTest<TestPacketStreamWithError>) {

	this->in.emplace(16_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);
	this->txIdW = 0_b;


	randomPackets(100, 1, 50 );

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}


using TestPacketStreamWithTxId = scl::RvPacketStream < BVec, scl::Empty, scl::TxId > ;

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_32_with_txid, PacketSendAndReceiveTest<TestPacketStreamWithTxId>) {

	this->txIdW = 4_b;

	this->in.emplace(16_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	txid(*in) = this->txIdW;
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

using TestPacketStreamWithEmptyBits= scl::RvPacketStream < BVec, scl::EmptyBits> ;

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_8_32_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(8_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_32_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(16_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthExtend(move(*this->in), 32_b);

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(extendWidthFuzz_16_48_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(16_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthExtend(move(*this->in), 48_b);

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_8, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(32_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthReduce(move(*this->in), 8_b);
	this->txIdW = 0_b;


	randomPackets(20, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}


BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_16, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(32_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);
	this->txIdW = 0_b;


	randomPackets(50, 1, 10);

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_48_16, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(48_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);
	this->txIdW = 0_b;


	randomPackets(50, 1, 15);

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_24_8, PacketSendAndReceiveTest<TestPacketStream>) {

	this->in.emplace(24_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthReduce(move(*this->in), 8_b);
	this->txIdW = 0_b;


	randomPackets(50, 1, 15);

	backpressureRNG = true;
	readyProbabilityPercent = 50;


	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_16_with_error, PacketSendAndReceiveTest<TestPacketStreamWithError>) {

	this->in.emplace(32_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);
	this->txIdW = 0_b;

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}


BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_16_with_txid, PacketSendAndReceiveTest<TestPacketStreamWithTxId>) {

	this->txIdW = 4_b;
	this->in.emplace(32_b);
	empty(*in) = BitWidth::last((*in)->width().bytes() - 1);
	txid(*in) = this->txIdW;
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_8_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(32_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthReduce(move(*this->in), 8_b);
	this->txIdW = 0_b;

	randomPackets(50, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_32_16_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(32_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);
	this->txIdW = 0_b;

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
}

BOOST_FIXTURE_TEST_CASE(reduceWidthFuzz_48_16_emptybits, PacketSendAndReceiveTest<TestPacketStreamWithEmptyBits>) {

	this->in.emplace(48_b);
	emptyBits(*in) = BitWidth::count((this->in)->data.width().bits());
	this->out = scl::strm::widthReduce(move(*this->in), 16_b);
	this->txIdW = 0_b;

	randomPackets(100, 1, 50);

	backpressureRNG = true;
	readyProbabilityPercent = 50;

	runTest(false);
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
			{
				if (backpressureRNG)
					fork([&, this]() -> SimProcess {
						std::mt19937 gen(1234567890);
						std::uniform_int_distribution<size_t> distrib(0, 99);
						while (true) {
							simu(ready(out)) = distrib(gen) < readyProbabilityPercent;
							co_await OnClk(packetTestclk);
						}
					});
				else
					simu(ready(out)) = '1';
			}
			

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

struct AppendTestSimulationFixture : public BoostUnitTestSimulationFixture 
{
	BitWidth dataW = 8_b;
	size_t iterations = 100;
	std::mt19937 rng;
	
	std::function<size_t()> headPacketSize = []() { return 4; };
	std::function<size_t()> tailPacketSize = []() { return 4; };
	std::function<size_t()> getHeadInvalidBeats = []() { return 0; };
	std::function<size_t()> getTailInvalidBeats = []() { return 0; };

	void runTest() {
		Clock clk({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScp(clk);
		using namespace gtry::scl;
		rng.seed(1234);

		RvPacketStream<BVec, EmptyBits> headStrm(dataW);
		emptyBits(headStrm) = BitWidth::count(headStrm->width().bits());
		pinIn(headStrm, "head");

		RvPacketStream<BVec, EmptyBits> tailStrm(dataW);
		emptyBits(tailStrm) = BitWidth::count(tailStrm->width().bits());
		pinIn(tailStrm, "tail");

		auto headToFunction = constructFrom(headStrm);
		headToFunction <<= headStrm; // ugly fix
		RvPacketStream<BVec, EmptyBits> out = strm::streamAppend(move(headToFunction), move(tailStrm));
		pinOut(out, "out");

		addSimulationProcess([&, this]()->SimProcess { return strm::readyDriverRNG(out, clk, 50); });
		auto makeRandomPacket = [&](size_t packetSizeInBits) {
			sim::DefaultBitVectorState state;
			state.resize(packetSizeInBits);
			state.setRange(sim::DefaultConfig::DEFINED, 0, state.size());
			for (auto& byte : state.asWritableBytes(sim::DefaultConfig::VALUE))
				byte = (std::byte) rng(); // somewhat wasteful, but simple
			return state;
			};

		std::vector<gtry::sim::DefaultBitVectorState> heads(iterations);
		for (auto& dbv : heads)
			dbv = makeRandomPacket(headPacketSize());
		std::vector<gtry::sim::DefaultBitVectorState> tails(iterations);
		for (auto& dbv : tails)
			dbv = makeRandomPacket(tailPacketSize());

		std::vector<gtry::sim::DefaultBitVectorState> results = copy(heads);
		for (size_t i = 0; i < iterations; i++)
			results[i].append(tails[i]);


		addSimulationProcess([&, this]()->SimProcess {
			for (auto head : heads) {
				co_await strm::sendPacket(headStrm, strm::SimPacket(head).invalidBeats(getHeadInvalidBeats()), clk);
			}
			});

		addSimulationProcess([&, this]()->SimProcess {
			
			for (auto tail : tails) {
				co_await strm::sendPacket(tailStrm, strm::SimPacket(tail).invalidBeats(getTailInvalidBeats()), clk);
				if (tail.size() == 0) {
					co_await performPacketTransferWait(headStrm, clk);
				}
			}
			});

		//receive and decode gatery tlp
		addSimulationProcess([&, this]()->SimProcess {
			for (size_t i = 0; i < iterations; i++)
			{
				strm::SimPacket packet = co_await strm::receivePacket(out, clk);
				BOOST_TEST(packet.payload == results[i]); //, "packet " + std::to_string(i) + " failed");
				BOOST_TEST(packet.payload == results[i], "packet " + std::to_string(i) + " failed");
			}
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			stopTest();
			});

		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 1000, 1'000'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(streamAppend_only_heads, AppendTestSimulationFixture)
{
	dataW = 8_b;
	iterations = 100;

	headPacketSize = [&]() { return (rng() & 0x1F) + 1; };
	tailPacketSize = []() { return 0; };
	runTest();
}

BOOST_FIXTURE_TEST_CASE(streamAppend_some_empty_tails, AppendTestSimulationFixture)
{
	dataW = 8_b;
	iterations = 100;

	headPacketSize = [&]() { return (rng() & 0x1F) + 1; };
	tailPacketSize = [&]() { return (rng() & 0x1); };
	runTest();
}

BOOST_FIXTURE_TEST_CASE(streamAppend_chaos, AppendTestSimulationFixture)
{
	dataW = 8_b;
	iterations = 1000;

	headPacketSize = [&]() { return (rng() & 0x3F) + 1; };
	tailPacketSize = [&]() { return (rng() & 0x1F); };
	runTest();
}


struct dropTailSimulationFixture : public BoostUnitTestSimulationFixture 
{
	BitWidth streamW = 8_b;
	size_t keep = 12;
	BitWidth maxPacketW = 64_b;
	size_t numPackets = 1000;

	std::mt19937 rng;

	std::function<sim::DefaultBitVectorState()> makePacket = [&]() {
		std::uniform_int_distribution<size_t> distrib(keep, maxPacketW.bits());
		return sim::createDefinedRandomDefaultBitVectorState(distrib(rng), rng);
	};

	void runTest() {
		rng.seed(1234);
		Clock clk({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScp(clk);

		scl::RvPacketStream<UInt, scl::EmptyBits> in(streamW);
		emptyBits(in) = BitWidth::count(in->width().bits());
		pinIn(in, "in");

		auto out = scl::strm::streamDropTail(move(in), keep, maxPacketW);

		pinOut(out, "out");

		addSimulationProcess([&, this]()->SimProcess {
			return scl::strm::readyDriverRNG(out, clk, 50);
			}); 

		std::vector<sim::DefaultBitVectorState> sentPackets(numPackets);

		for (auto& packet: sentPackets)
			packet = makePacket();

		addSimulationProcess([&, this]()->SimProcess {
			for (auto& packet : sentPackets)
				co_await scl::strm::sendPacket(in, scl::strm::SimPacket(packet), clk);
			co_await OnClk(clk);
			});

		addSimulationProcess([&, this]()->SimProcess {
			for (auto& packet : sentPackets) {
				auto receivedPacket = co_await scl::strm::receivePacket(out, clk);
				if (keep <= packet.size())
					BOOST_TEST(receivedPacket.payload == packet.extract(0, keep));
				else
					BOOST_TEST(receivedPacket.payload == packet);
			}
			stopTest();
			});	


		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
	}
};


BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static, dropTailSimulationFixture)
{
	streamW = 8_b;
	keep = 12;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
} 

BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static_one_beat, dropTailSimulationFixture)
{
	streamW = 8_b;
	keep = 4;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static_edge_case, dropTailSimulationFixture)
{
	streamW = 8_b;
	keep = 16;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static_keep_max, dropTailSimulationFixture)
{
	streamW = 8_b;
	keep = 64;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static_nonpow2, dropTailSimulationFixture)
{
	streamW = 12_b;
	keep = 64;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}


//should fail
BOOST_FIXTURE_TEST_CASE(stream_drop_tail_static_small_packet, dropTailSimulationFixture, *boost::unit_test::disabled())
{
	streamW = 8_b;
	keep = 12;
	maxPacketW = 64_b;
	numPackets = 100;

	makePacket = [&]() {
		return sim::createDefinedRandomDefaultBitVectorState(keep / 2, rng);
	};
	runTest();
}



struct MyMeta {
	BOOST_HANA_DEFINE_STRUCT(MyMeta,
		(UInt, myMeta)
	);
};

struct AddStreamAsMetaDataSimulationFixture : public BoostUnitTestSimulationFixture 
{
	BitWidth streamW = 8_b;
	BitWidth maxPacketW = 64_b;
	size_t numPackets = 1000;

	std::mt19937 rng;

	sim::DefaultBitVectorState makePacket(std::mt19937 &rng, size_t idx) {
		std::uniform_int_distribution<size_t> distrib(4, maxPacketW.bits());
		return sim::createDefaultBitVectorState(distrib(rng), idx);
	}

	void runTest() {
		rng.seed(1234);
		Clock clk({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScp(clk);

		scl::RvPacketStream<UInt, scl::EmptyBits> in(streamW);
		emptyBits(in) = BitWidth::count(in->width().bits());
		pinIn(in, "in");

		scl::RvStream<UInt> metaIn(16_b);
		pinIn(metaIn, "metaIn");

		//auto wrappped = metaIn.transform([](const auto &v) { return MyMeta{ v }; });
		//HCL_NAMED(wrappped);
		//auto out = move(in).add(move(wrappped));

		auto out = move(in).addAs<MyMeta>(move(metaIn));
		pinOut(out, "out");

		addSimulationProcess([&, this]()->SimProcess {
			fork([&, this]()->SimProcess {
				co_await scl::strm::readyDriverRNG(out, clk, 50);
			}); 

			std::vector<sim::DefaultBitVectorState> sentPackets(numPackets);

			for (size_t idx = 0; idx < sentPackets.size(); idx++)
				sentPackets[idx] = makePacket(rng, idx);

			std::vector<size_t> sentMetaData(numPackets);
			for (size_t idx = 0; idx < sentMetaData.size(); idx++)
				sentMetaData[idx] = idx;


			fork([&, this]()->SimProcess {
				for (auto& packet : sentPackets)
					co_await scl::strm::sendPacket(in, scl::strm::SimPacket(packet), clk);
			});

			fork([&, this]()->SimProcess {
				co_await OnClk(clk);
				for (auto& d : sentMetaData) {
					simu(*metaIn) = d;
					co_await scl::strm::performTransferWait(metaIn, clk);
				}
				simu(*metaIn).invalidate();
			});

			fork([&, this]()->SimProcess {
				simu(valid(metaIn)) = '0';
				co_await OnClk(clk);
				while (true) {
					if (rng() & 1) {
						simu(valid(metaIn)) = '1';
						co_await scl::strm::performTransferWait(metaIn, clk);
						simu(valid(metaIn)) = '0';
					} else
						co_await OnClk(clk);
				}
			});



			for (auto& d : sentMetaData) {
				scl::strm::SimuStreamPerformTransferWait<decltype(out)> streamTransfer;
				do {
					co_await streamTransfer.wait(out, clk);
					BOOST_TEST(simu(out.template get<MyMeta>().myMeta) == d);
				} while (!simuEop(out));
			}
			stopTest();
		});


		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
	}
};


BOOST_FIXTURE_TEST_CASE(AddStreamAsMetaData, AddStreamAsMetaDataSimulationFixture)
{
	streamW = 8_b;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}


struct AddMetaSignalFromPacketSimulationFixture : public BoostUnitTestSimulationFixture 
{
	BitWidth streamW = 8_b;
	BitWidth maxPacketW = 64_b;
	size_t numPackets = 1000;

	std::mt19937 rng;

	sim::DefaultBitVectorState makePacket(std::mt19937 &rng, size_t idx) {
		std::uniform_int_distribution<size_t> distrib(4, maxPacketW.bits());
		return sim::createDefaultBitVectorState(distrib(rng), idx);
	}

	void runTest() {
		rng.seed(1234);
		Clock clk({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScp(clk);

		scl::RvPacketStream<UInt, scl::EmptyBits> in(streamW);
		emptyBits(in) = BitWidth::count(in->width().bits());
		pinIn(in, "in");

		scl::RvPacketStream<UInt, scl::EmptyBits, MyMeta> out = move(in) | 
					scl::strm::addMetaSignalFromPacket(maxPacketW.bits() / streamW.bits() + 1, 
								[&](scl::RvPacketStream<UInt, scl::EmptyBits> &&in) {
									return scl::strm::packetSize(move(in), maxPacketW).transform([](const UInt &size) { return MyMeta{ size }; }) | scl::strm::regDownstream();
								});

		pinOut(out, "out");

		addSimulationProcess([&, this]()->SimProcess {
			fork([&, this]()->SimProcess {
				co_await scl::strm::readyDriverRNG(out, clk, 50);
			}); 

			std::vector<sim::DefaultBitVectorState> sentPackets(numPackets);

			for (size_t idx = 0; idx < sentPackets.size(); idx++)
				sentPackets[idx] = makePacket(rng, idx);


			fork([&, this]()->SimProcess {
				for (auto& packet : sentPackets)
					co_await scl::strm::sendPacket(in, scl::strm::SimPacket(packet), clk);
			});


			fork([&, this]()->SimProcess {
				for (auto& packet : sentPackets) {
					auto receivedPacket = co_await scl::strm::receivePacket(out, clk);
					BOOST_TEST(receivedPacket.payload == packet);
				}
			});	

			for (auto& packet : sentPackets) {
				scl::strm::SimuStreamPerformTransferWait<decltype(out)> streamTransfer;
				do {
					co_await streamTransfer.wait(out, clk);
					BOOST_TEST(simu(out.template get<MyMeta>().myMeta) == packet.size());
				} while (!simuEop(out));
			}
			stopTest();
		});


		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(AddMetaSignalFromPacket, AddMetaSignalFromPacketSimulationFixture)
{
	streamW = 8_b;
	maxPacketW = 64_b;
	numPackets = 100;
	runTest();
}
