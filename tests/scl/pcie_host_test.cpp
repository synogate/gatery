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

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/sim/PcieHostModel.h>
#include <gatery/scl/sim/SimPci.h>


#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/export/DotExport.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;


BOOST_FIXTURE_TEST_CASE(host_read_1dw_512_b, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 512_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 16;
	static_assert(memSizeInBytes % 4 == 0);

	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	host.defaultHandlers();
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 1;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });
	addSimulationProcess([&, this]()->SimProcess {
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");
		
		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		gtry::sim::DefaultBitVectorState tlpPayload = responsePacket.payload.extract(96, responsePacket.payload.size() - 96);

		BOOST_TEST(host.memory().read(*read.wordAddress << 5, *read.length << 5) == tlpPayload);

		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
		BOOST_TEST(*tlp.byteCount == 4);
		BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::successfulCompletion));
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
	});
	
	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}



BOOST_FIXTURE_TEST_CASE(host_read_64dw_512_b, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 512_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 128;
	static_assert(memSizeInBytes % 4 == 0);


	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	host.defaultHandlers();
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 64 >> 2;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });
	addSimulationProcess([&, this]()->SimProcess {

		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");

		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		gtry::sim::DefaultBitVectorState tlpPayload = responsePacket.payload.extract(96, responsePacket.payload.size() - 96);

		BOOST_TEST(tlpPayload == host.memory().read(*read.wordAddress << 5, *read.length << 5));
		
		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
		BOOST_TEST(*tlp.byteCount == 64);
		BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::successfulCompletion));
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(host_read_chunks64B_1dw_512_b, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 512_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 128;
	static_assert(memSizeInBytes % 4 == 0);

	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	host.updateHandler(pci::TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(64));
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 1;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });
	addSimulationProcess([&, this]()->SimProcess {
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");

		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		gtry::sim::DefaultBitVectorState tlpPayload = responsePacket.payload.extract(96, responsePacket.payload.size() - 96);

		BOOST_TEST(tlpPayload == host.memory().read(*read.wordAddress << 5, *read.length << 5));

		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
		BOOST_TEST(*tlp.byteCount == 4);
		BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::successfulCompletion));
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(host_read_chunks64B_16dw_512_b, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 512_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 128;
	static_assert(memSizeInBytes % 4 == 0);


	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	host.updateHandler(pci::TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(64));
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 16;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });
	addSimulationProcess([&, this]()->SimProcess {
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");

		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		gtry::sim::DefaultBitVectorState tlpPayload = responsePacket.payload.extract(96, responsePacket.payload.size() - 96);

		BOOST_TEST(tlpPayload == host.memory().read(*read.wordAddress << 5, *read.length << 5));

		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
		BOOST_TEST(*tlp.byteCount == 64);
		BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::successfulCompletion));
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(host_read_chunks64B_17dw_512_b, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 512_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 128;
	static_assert(memSizeInBytes % 4 == 0);


	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	size_t bytesPerChunk = 64;
	host.updateHandler(pci::TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(bytesPerChunk));
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 17;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });

	size_t bitsLeft = *read.length << 5;
	size_t bitAddress = *read.wordAddress << 5;
	addSimulationProcess([&, this]()->SimProcess {	
		size_t numberOfReponsePackets = (*read.length << 2) / bytesPerChunk + 1;
		for (size_t i = 0; i < numberOfReponsePackets; i++)
		{
			gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
			scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
			gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");

			auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
			gtry::sim::DefaultBitVectorState tlpPayload = responsePacket.payload.extract(96, responsePacket.payload.size() - 96);

			BOOST_TEST(tlpPayload == host.memory().read(bitAddress, std::min(bitsLeft, bytesPerChunk * 8)));
			BOOST_TEST(*tlp.byteCount == (bitsLeft / 8));
			bitAddress += bytesPerChunk * 8;
			bitsLeft -= bytesPerChunk * 8;

			BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
			BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::successfulCompletion));
		}
		
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(host_unsupported_completer, BoostUnitTestSimulationFixture) {
	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);
	BitWidth streamW = 256_b;

	pci::TlpPacketStream<EmptyBits> requesterRequest(streamW);
	emptyBits(requesterRequest) = BitWidth::count(requesterRequest->width().bits());
	pinIn(requesterRequest, "rr_in");

	const size_t memSizeInBytes = 4;
	static_assert(memSizeInBytes % 4 == 0);


	scl::sim::RandomBlockDefinition testSpace{
		.offset = 0,
		.size = memSizeInBytes * 8,
		.seed = 1234,
	};

	scl::sim::PcieHostModel host(testSpace);
	//host.defaultHandlers(); don't set any support
	host.requesterRequest(move(requesterRequest));

	pci::TlpPacketStream<EmptyBits>& requesterCompletion = host.requesterCompletion();
	pinOut(requesterCompletion, "rc_out");

	scl::sim::TlpInstruction read;
	read.opcode = pci::TlpOpcode::memoryReadRequest64bit;
	read.wordAddress = 0;
	read.length = 1;
	read.lastDWByteEnable = 0;

	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 3); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriver(requesterCompletion, clk); });
	addSimulationProcess([&, this]()->SimProcess { return scl::strm::sendPacket(requesterRequest, scl::strm::SimPacket{ read }, clk); });
	addSimulationProcess([&, this]()->SimProcess {

		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Awaiting response packet");
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		gtry::sim::SimulationContext::current()->onDebugMessage(nullptr, "Got a response packet");

		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithoutData));
		BOOST_TEST((tlp.completionStatus == pci::CompletionStatus::unsupportedRequest));
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}




