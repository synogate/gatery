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

#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/adaptWidth.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/stream/PacketStreamHelpers.h>
#include <gatery/scl/io/SpiMaster.h> 

#include <gatery/debug/websocks/WebSocksInterface.h>
#include <gatery/scl/sim/SimulationSequencer.h>


using namespace boost::unit_test;
using namespace gtry;


template<typename StreamType>
struct PacketSendAndReceiveTest : public BoostUnitTestSimulationFixture
{
	Clock packetTestclk = Clock({ .absoluteFrequency = 100'000'000 });
	std::vector<scl::SimPacket> allPackets;
	bool addPipelineReg = true;
	BitWidth txIdSize = 4_b;
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
			txid(in) = txIdSize;
			txid(out) = txIdSize;
		}

		if (addPipelineReg)
			out <<= in.regDownstream();
		else
			out <<= in;

		pinIn(in, "in_");
		pinOut(out, "out_");


		addSimulationProcess([&, this]()->SimProcess {
			scl::SimulationSequencer sendingSequencer;
			for (const auto& packet : allPackets)
				fork(sendPacket(in, packet, packetTestclk, sendingSequencer));

			for (const auto& packet : allPackets) {
				scl::SimPacket rvdPacket = co_await scl::receivePacket(out, packetTestclk, unreadyMask);
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
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_multiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_longMultiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_packetStream, PacketSendAndReceiveTest<scl::PacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	addPipelineReg = false;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_VPacketStream, PacketSendAndReceiveTest<scl::VPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream, PacketSendAndReceiveTest<scl::RsPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_SPacketStream, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream_bubbles, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream_bubbles_backpressure, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	unreadyMask = 0b10110001101;
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}


using RsePacketStream = scl::RsPacketStream<BVec, scl::Empty>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty, PacketSendAndReceiveTest<RsePacketStream>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }),
	};
	runTest();
}


using RseePacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::Error>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_error, PacketSendAndReceiveTest<RseePacketStream>) {

	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).error(false),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).error(true),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).error(false),
		scl::SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).error(true),
	};
	runTest();
}

using RsetPacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::TxId>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_txid, PacketSendAndReceiveTest<RsetPacketStream>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).txid(0),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).txid(1),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).txid(2),
		scl::SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).txid(0),
	};
	runTest();
}
BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_imposing_txid_with_no_support_RvPacketStream, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).txid(0),
		scl::SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).txid(2),
		scl::SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).txid(1),
		scl::SimPacket(std::vector<uint8_t>{ 0x30, 0x31 }).txid(0),
	};
	runTest();
}

using RsePacketStream = scl::RsPacketStream<BVec, scl::Empty>;
BOOST_FIXTURE_TEST_CASE(packetSenderFramework_test_RvStream, PacketSendAndReceiveTest<RsePacketStream>) {
	allPackets = std::vector<scl::SimPacket>{
		scl::SimPacket{0xABCD , 16_b},
		scl::SimPacket{0xABCD , 32_b},
		scl::SimPacket{0xABCD , 40_b},
		scl::SimPacket{0xABCD , 50_b},
		scl::SimPacket{0xABCD , 60_b},
		scl::SimPacket{0xABCD , 100_b},
	};

	runTest();
}


