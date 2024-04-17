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
#include <iostream>
#include <iomanip>

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/sim/SimPci.h> 
#include <gatery/scl/io/pci/PciToTileLink.h> 
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/sim/PcieHostModel.h>
#include <gatery/scl/io/pci/pciDispatcher.h>
#include <gatery/scl/io/pci/PciInterfaceSplitter.h>

using namespace boost::unit_test;
using namespace gtry;
BOOST_FIXTURE_TEST_CASE(tlp_builder_test, BoostUnitTestSimulationFixture)
{
	using namespace scl::pci;
	using namespace scl::sim;
	//Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	//ClockScope clkScope(clk);
	TlpInstruction read{
		.opcode = TlpOpcode::memoryReadRequest64bit,
		.lastDWByteEnable = 0,
		.wordAddress = 0x0123456789ABCDEC,
	};
	read.safeLength(1);
	auto dbv = (sim::DefaultBitVectorState)read;

	//std::cout << "READ" << std::endl;
	for (size_t byte = 0; byte < dbv.size() / 8; byte++) {
		// if (byte % 4 == 0) std::cout << std::endl;
		//std::cout << std::setw(10) << std::dec << dbv.extract(byte * 8, 8) << " " << std::setw(2) << std::hex << dbv.extract(byte * 8, 8) << std::endl;
	}
	//convert back to TlpInstruction and check equivalence
	auto recreatedRead = TlpInstruction::createFrom(dbv);
	BOOST_TEST((recreatedRead == read));

	TlpInstruction write{
		.opcode = TlpOpcode::memoryWriteRequest64bit,
		.requesterID = 0xABCD,
		.tag = 0xFF,
		.lastDWByteEnable = 0,
		.wordAddress = 0x0123456789ABCDEC,
	};
	write.safeLength(1);
	write.payload = std::vector<uint32_t>{ 0xAAAAAAAA };

	dbv = (sim::DefaultBitVectorState)write;
	//std::cout << "WRITE" << std::endl;
	for (size_t byte = 0; byte < dbv.size() / 8; byte++) {
		// if (byte % 4 == 0) std::cout << std::endl;
		//std::cout << std::setw(8) << std::dec << dbv.extract(byte * 8, 8) << " " << std::setw(2) << std::hex << dbv.extract(byte * 8, 8) << std::endl;
	}

	//check equivalence after round trip
	auto recreatedWrite = TlpInstruction::createFrom(dbv);
	BOOST_TEST((recreatedWrite == write));
}

scl::strm::SimPacket writeWord(size_t byteAddress, uint32_t data) {
	HCL_DESIGNCHECK_HINT(byteAddress % 4 == 0, "the address must be word aligned");
	scl::sim::TlpInstruction write{
		.opcode = scl::pci::TlpOpcode::memoryWriteRequest64bit,
		.length = 1,
		.requesterID = 0xABCD,
		.tag = 0xFF,
		.lastDWByteEnable = 0,
		.wordAddress = byteAddress >> 2,
	};
	write.payload = std::vector<uint32_t>{ data };
	return scl::strm::SimPacket(write);
}

scl::strm::SimPacket readWord(size_t byteAddress) {
	HCL_DESIGNCHECK_HINT(byteAddress % 4 == 0, "the address must be word aligned");
	scl::sim::TlpInstruction read{
		.opcode = scl::pci::TlpOpcode::memoryReadRequest64bit,
		.length = 1,
		.requesterID = 0xABCD,
		.tag = 0xFF,
		.lastDWByteEnable = 0,
		.wordAddress = byteAddress >> 2,
	};
	return scl::strm::SimPacket(read);
}


BOOST_FIXTURE_TEST_CASE(tlp_RequestHeader_test, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	using namespace scl::sim;

	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	std::mt19937 rng{ std::random_device{}() };

	TlpInstruction read = TlpInstruction::randomizeNaive(TlpOpcode::memoryReadRequest64bit, std::random_device{}() );

	BVec rawHeader = 128_b;
	pinIn(rawHeader, "in_raw", {.simulationOnlyPin = true});

	RequestHeader reqHdr = RequestHeader::fromRaw(rawHeader);

	pinOut(reqHdr, "out");

	BVec reconstructed = (BVec)reqHdr;

	RequestHeader reqHdrRecon = RequestHeader::fromRaw(reconstructed);

	pinOut(reqHdrRecon, "out");


	addSimulationProcess(
		[&, this]()->SimProcess {
			co_await OnClk(clk);
			simu(rawHeader) = (sim::DefaultBitVectorState) read;
			co_await WaitFor(Seconds{ 0, 1});
			BOOST_TEST(simu(reqHdr.common.type) == ((size_t) read.opcode & 0x1Fu));
			BOOST_TEST(simu(reqHdr.common.fmt) == ((size_t) read.opcode >> 5));
			BOOST_TEST(simu(reqHdr.common.addressType) == read.at);
			BOOST_TEST(simu(reqHdr.processingHint) == read.ph);
			BOOST_TEST(simu(reqHdr.common.processingHintPresence) == read.th);
			BOOST_TEST(simu(reqHdr.common.attributes.idBasedOrdering) == read.idBasedOrderingAttr2);
			BOOST_TEST(simu(reqHdr.common.attributes.noSnoop) == read.noSnoopAttr0);
			BOOST_TEST(simu(reqHdr.common.attributes.relaxedOrdering) == read.relaxedOrderingAttr1);
			BOOST_TEST(simu(reqHdr.common.digest) == read.td);
			BOOST_TEST(simu(reqHdr.common.poisoned) == read.ep);
			BOOST_TEST(simu(reqHdr.common.length) == *read.length);
			BOOST_TEST(simu(reqHdr.common.trafficClass) == read.tc);

			BOOST_TEST(simu(reqHdr.firstDWByteEnable) == read.firstDWByteEnable);
			BOOST_TEST(simu(reqHdr.lastDWByteEnable) == read.lastDWByteEnable);
			BOOST_TEST(simu(reqHdr.requesterId) == read.requesterID);
			BOOST_TEST(simu(reqHdr.tag) == read.tag);
			BOOST_TEST(simu(reqHdr.wordAddress) == *read.wordAddress);

			BOOST_TEST(simu(reqHdrRecon.common.type) == simu(reqHdr.common.type));
			BOOST_TEST(simu(reqHdrRecon.common.fmt)  == simu(reqHdr.common.fmt));
			BOOST_TEST(simu(reqHdrRecon.common.addressType)  == simu(reqHdr.common.addressType));
			BOOST_TEST(simu(reqHdrRecon.processingHint)  == simu(reqHdr.processingHint));
			BOOST_TEST(simu(reqHdrRecon.common.processingHintPresence)  == simu(reqHdr.common.processingHintPresence));
			BOOST_TEST(simu(reqHdrRecon.common.attributes.idBasedOrdering)  == simu(reqHdr.common.attributes.idBasedOrdering));
			BOOST_TEST(simu(reqHdrRecon.common.attributes.noSnoop)  == simu(reqHdr.common.attributes.noSnoop));
			BOOST_TEST(simu(reqHdrRecon.common.attributes.relaxedOrdering)  == simu(reqHdr.common.attributes.relaxedOrdering));
			BOOST_TEST(simu(reqHdrRecon.common.digest)  == simu(reqHdr.common.digest));
			BOOST_TEST(simu(reqHdrRecon.common.poisoned)  == simu(reqHdr.common.poisoned));
			BOOST_TEST(simu(reqHdrRecon.common.length)  == simu(reqHdr.common.length));
			BOOST_TEST(simu(reqHdrRecon.common.trafficClass)  == simu(reqHdr.common.trafficClass));

			BOOST_TEST(simu(reqHdrRecon.firstDWByteEnable) == simu(reqHdr.firstDWByteEnable));
			BOOST_TEST(simu(reqHdrRecon.lastDWByteEnable) == simu(reqHdr.lastDWByteEnable));
			BOOST_TEST(simu(reqHdrRecon.requesterId) == simu(reqHdr.requesterId));
			BOOST_TEST(simu(reqHdrRecon.tag) == simu(reqHdr.tag));
			BOOST_TEST(simu(reqHdrRecon.wordAddress) == simu(reqHdr.wordAddress));

			co_await OnClk(clk);
			stopTest();
		}
	);

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({1,1'000'000}));
}

BOOST_FIXTURE_TEST_CASE(tlp_CompletionHeader_test, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	using namespace scl::sim;

	Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScope(clk);

	BVec rawHeader = 96_b;
	pinIn(rawHeader, "in_raw");

	CompletionHeader compHdr = CompletionHeader::fromRaw(rawHeader);

	pinOut(compHdr, "out");

	BVec reconstructed = (BVec)compHdr;
	
	CompletionHeader compHdrRecon = CompletionHeader::fromRaw(reconstructed);
	
	pinOut(compHdrRecon, "out");


	addSimulationProcess(
		[&, this]()->SimProcess {
			for (size_t i = 0; i < 100; i++)
			{
				TlpInstruction comp = TlpInstruction::randomizeNaive(TlpOpcode::completionWithData, i);
				co_await OnClk(clk);
				simu(rawHeader) = (sim::DefaultBitVectorState)comp;
				co_await WaitFor(Seconds{ 0, 1 });
				BOOST_TEST(simu(compHdr.common.type) == ((size_t)comp.opcode & 0x1Fu));
				BOOST_TEST(simu(compHdr.common.fmt) == ((size_t)comp.opcode >> 5));
				BOOST_TEST(simu(compHdr.common.addressType) == comp.at);
				BOOST_TEST(simu(compHdr.common.processingHintPresence) == comp.th);
				BOOST_TEST(simu(compHdr.common.attributes.idBasedOrdering) == comp.idBasedOrderingAttr2);
				BOOST_TEST(simu(compHdr.common.attributes.noSnoop) == comp.noSnoopAttr0);
				BOOST_TEST(simu(compHdr.common.attributes.relaxedOrdering) == comp.relaxedOrderingAttr1);
				BOOST_TEST(simu(compHdr.common.digest) == comp.td);
				BOOST_TEST(simu(compHdr.common.poisoned) == comp.ep);
				BOOST_TEST(simu(compHdr.common.length) == *comp.length);
				BOOST_TEST(simu(compHdr.common.trafficClass) == comp.tc);


				BOOST_TEST(simu(compHdr.requesterId) == comp.requesterID);
				BOOST_TEST(simu(compHdr.completerId) == *comp.completerID);
				BOOST_TEST(simu(compHdr.tag) == comp.tag);
				BOOST_TEST(simu(compHdr.completionStatus) == (size_t)comp.completionStatus);
				if (comp.completionStatus == CompletionStatus::successfulCompletion) {
					BOOST_TEST(simu(compHdr.byteCount) == *comp.byteCount);
					BOOST_TEST(simu(compHdr.byteCountModifier) == comp.byteCountModifier);
					BOOST_TEST(simu(compHdr.lowerByteAddress) == *comp.lowerByteAddress);
				}

				BOOST_TEST(simu(compHdrRecon.common.type) == simu(compHdr.common.type));
				BOOST_TEST(simu(compHdrRecon.common.fmt) == simu(compHdr.common.fmt));
				BOOST_TEST(simu(compHdrRecon.common.addressType) == simu(compHdr.common.addressType));
				BOOST_TEST(simu(compHdrRecon.common.processingHintPresence) == simu(compHdr.common.processingHintPresence));
				BOOST_TEST(simu(compHdrRecon.common.attributes.idBasedOrdering) == simu(compHdr.common.attributes.idBasedOrdering));
				BOOST_TEST(simu(compHdrRecon.common.attributes.noSnoop) == simu(compHdr.common.attributes.noSnoop));
				BOOST_TEST(simu(compHdrRecon.common.attributes.relaxedOrdering) == simu(compHdr.common.attributes.relaxedOrdering));
				BOOST_TEST(simu(compHdrRecon.common.digest) == simu(compHdr.common.digest));
				BOOST_TEST(simu(compHdrRecon.common.poisoned) == simu(compHdr.common.poisoned));
				BOOST_TEST(simu(compHdrRecon.common.length) == simu(compHdr.common.length));
				BOOST_TEST(simu(compHdrRecon.common.trafficClass) == simu(compHdr.common.trafficClass));

				BOOST_TEST(simu(compHdrRecon.requesterId) == simu(compHdr.requesterId));
				BOOST_TEST(simu(compHdrRecon.tag) == simu(compHdr.tag));
				BOOST_TEST(simu(compHdrRecon.completerId) == simu(compHdr.completerId));
				BOOST_TEST(simu(compHdrRecon.byteCount) == simu(compHdr.byteCount));
				BOOST_TEST(simu(compHdrRecon.byteCountModifier) == simu(compHdr.byteCountModifier));
				BOOST_TEST(simu(compHdrRecon.lowerByteAddress) == simu(compHdr.lowerByteAddress));
				BOOST_TEST(simu(compHdrRecon.completionStatus) == simu(compHdr.completionStatus));

				co_await OnClk(clk);
			}
			stopTest();
		}
	);

	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 3, 1'000'000 }));
}



BOOST_FIXTURE_TEST_CASE(tlp_to_tilelink_rw64_1dw, BoostUnitTestSimulationFixture) {
	using namespace scl::pci;
	using namespace scl::sim;
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);


	BitWidth tlAddrW = 5_b;
	BitWidth tlDataW = 32_b;
	BitWidth tlSourceW = pack(TlpAnswerInfo{}).width();
	BitWidth tlpW = 512_b;

	Memory<BVec> mem(tlAddrW.count(), BVec(tlDataW));
	scl::TileLinkUL ul = scl::tileLinkInit<scl::TileLinkUL>(tlAddrW, tlDataW, tlSourceW);
	mem <<= ul;

	HCL_NAMED(ul);


	auto completerInterface = makeTileLinkMaster(move(ul), tlpW);
	TlpPacketStream<scl::EmptyBits, BarInfo>& in = completerInterface.request;
	in.set(BarInfo{ ConstBVec(0, 3_b) , 12});
	pinIn(in, "in");

	TlpPacketStream<scl::EmptyBits>& out = completerInterface.completion;
	pinOut(out, "out");

	setName((*out)(96, 32_b), "out_payload");
	setName((*out)(0, 96_b), "out_hdr");

	addSimulationProcess([&, this]()->SimProcess {
		co_await scl::strm::sendPacket(in, writeWord(0, 0xAAAAAAAA), clk);
		co_await scl::strm::sendPacket(in, writeWord(4, 0xBBBBBBBB), clk);
		co_await scl::strm::sendPacket(in, writeWord(8, 0xCCCCCCCC), clk);
		co_await scl::strm::sendPacket(in, readWord(0), clk);
		co_await scl::strm::sendPacket(in, readWord(4), clk);
		co_await scl::strm::sendPacket(in, readWord(8), clk);
		
	});

	addSimulationProcess([&, this]() { return scl::strm::readyDriver(out, clk); });
	addSimulationProcess([&, this]()->SimProcess{
		auto firstPacket = co_await scl::strm::receivePacket(out, clk);
		TlpInstruction receivedTlp = TlpInstruction::createFrom(firstPacket.payload);
		BOOST_TEST((receivedTlp.opcode == TlpOpcode::completionWithData));
		BOOST_TEST((receivedTlp.requesterID == 0xABCD));
		BOOST_TEST((receivedTlp.tag == 0xFF));
		BOOST_TEST((receivedTlp.length == 1));
		BOOST_TEST((receivedTlp.payload->at(0) == 0xAAAAAAAA));

		firstPacket = co_await scl::strm::receivePacket(out, clk);
		receivedTlp = TlpInstruction::createFrom(firstPacket.payload);
		BOOST_TEST((receivedTlp.opcode == TlpOpcode::completionWithData));
		BOOST_TEST((receivedTlp.requesterID == 0xABCD));
		BOOST_TEST((receivedTlp.tag == 0xFF));
		BOOST_TEST((receivedTlp.length == 1));
		BOOST_TEST((receivedTlp.payload->at(0) == 0xBBBBBBBB));


		firstPacket = co_await scl::strm::receivePacket(out, clk);
		receivedTlp = TlpInstruction::createFrom(firstPacket.payload);
		BOOST_TEST((receivedTlp.opcode == TlpOpcode::completionWithData));
		BOOST_TEST((receivedTlp.requesterID == 0xABCD));
		BOOST_TEST((receivedTlp.tag == 0xFF));
		BOOST_TEST((receivedTlp.length == 1));
		BOOST_TEST((receivedTlp.payload->at(0) == 0xCCCCCCCC));

		co_await OnClk(clk);
		stopTest();
		});


	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}



BOOST_FIXTURE_TEST_CASE(pci_deadbeef_test, BoostUnitTestSimulationFixture) {
	using namespace scl::pci;
	using namespace scl::sim;
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const uint64_t deadbeef = 0xABCD'DEAD'BEEF'ABCDull;

	UInt addr = ConstUInt(deadbeef, 64_b);

	RequestHeader hdr;
	//hdr.wordAddress = addr >> 2; this line silently resizes the vector 
	hdr.wordAddress = zext(addr.upper(-2_b));

	BOOST_TEST(hdr.wordAddress.width() == 62_b);

	BVec raw = (BVec)hdr;

	RequestHeader reconstructed = hdr.fromRaw(raw);
	addSimulationProcess([&, this]()->SimProcess {
		co_await OnClk(clk);
			BOOST_TEST(simu(reconstructed.wordAddress) == simu(hdr.wordAddress));
			stopTest();
		}
	);

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(pci_requesterCompletion_tileLink_fullW_test, BoostUnitTestSimulationFixture) {
	using namespace scl::pci;
	using namespace scl::sim;
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	size_t payloadBits = 512;

	TlpInstruction reqComp;
	reqComp.opcode = TlpOpcode::completionWithData;
	reqComp.byteCount	= payloadBits / 8;
	reqComp.length		= payloadBits / 32;
	reqComp.completerID = 0x5678;
	reqComp.lowerByteAddress = 0x7A;
	reqComp.payload = { 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF, 0x01234567, 0x89ABCDEF };

	scl::strm::SimPacket reqCompPacket(reqComp);
	reqCompPacket.invalidBeats(0b01011100101);


	TlpPacketStream<scl::EmptyBits> rc(512_b);
	emptyBits(rc) = BitWidth::count(rc->width().bits());
	pinIn(rc, "rc");

	scl::TileLinkChannelD d = scl::pci::requesterCompletionToTileLinkDFullW(move(rc));
	pinOut(d, "d");

	BOOST_TEST(d->data.width() == rc->width());

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriverRNG(d, clk, 50); });

	size_t iterations = 4;
	addSimulationProcess([&, this]()->SimProcess {
		scl::SimulationSequencer seq;
		for (size_t i = 0; i < iterations; i++)
		{
			fork(scl::strm::sendPacket(rc, reqCompPacket, clk, seq));
		}

		for (size_t i = 0; i < iterations; i++)
		{
			co_await scl::strm::performTransferWait(d, clk);
			BOOST_TEST(simu(d->source) == reqComp.tag);
			BOOST_TEST(simu(d->size) == 6);
			BOOST_TEST((simu(d->data) == reqComp.payload));
		}

		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		}
	);

	if (false) recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(pci_requester_512bit_tilelink_to_hostModel_test, BoostUnitTestSimulationFixture)
{
	Area area{ "top_area", true };
	using namespace scl::pci;
	const Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t memSizeInBytes = 64;

	scl::sim::PcieHostModel host(scl::sim::RandomBlockDefinition{ 0, memSizeInBytes * 8, 1039834 });
	host.defaultHandlers();
	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 0, 50); });

	scl::TileLinkUL master = makePciMasterFullW(host.requesterInterface(512_b));

	pinIn(master, "link");

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriverRNG(*master.d, clk, 50); });

	addSimulationProcess(
		[&, this]()->SimProcess {
			simu(valid(master.a)) = '0';
			co_await OnClk(clk);
			simu(master.a->address) = 0;
			simu(master.a->opcode) = (size_t) scl::TileLinkA::OpCode::Get;
			simu(master.a->param) = 0;
			simu(master.a->source) = 42;
			simu(master.a->size) = 6;
			simu(valid(master.a)) = '1';

			co_await scl::strm::performTransferWait(master.a, clk);
			simu(valid(master.a)) = '0';
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			co_await OnClk(clk);
			simu(master.a->address) = 0;
			simu(master.a->opcode) = (size_t) scl::TileLinkA::OpCode::Get;
			simu(master.a->param) = 0;
			simu(master.a->source) = 24;
			simu(master.a->size) = 6;
			simu(valid(master.a)) = '1';

			co_await scl::strm::performTransferWait(master.a, clk);
			simu(valid(master.a)) = '0';
		}
	);

	addSimulationProcess(
		[&, this]()->SimProcess {

			co_await scl::strm::performTransferWait(*master.d, clk);
			BOOST_TEST(host.memory().read(0,512) == (sim::DefaultBitVectorState) simu((*master.d)->data));
			BOOST_TEST(simu((*master.d)->source) == 42);

			co_await scl::strm::performTransferWait(*master.d, clk);
			BOOST_TEST(host.memory().read(0,512) == (sim::DefaultBitVectorState) simu((*master.d)->data));
			BOOST_TEST(simu((*master.d)->source) == 24);
			co_await OnClk(clk);
			stopTest();
		}
	);

	if (false) recordVCD("dut.vcd");
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(pci_requester_512bit_cheapBurst_test_rng_backpressure, BoostUnitTestSimulationFixture)
{
	Area area{ "top_area", true };
	using namespace scl::pci;
	const Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t memSizeInBytes = 1 << 10; // 1kB space

	scl::sim::PcieHostModel host(scl::sim::RandomBlockDefinition{ 0, memSizeInBytes * 8, 1039834});
	host.defaultHandlers();
	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 0, 50); });

	scl::TileLinkUB master = makePciMasterCheapBurst(host.requesterInterface(512_b), {}, 4_b);

	pinIn(master, "link");

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriverRNG(*master.d, clk, 50); });

	addSimulationProcess(
		[&, this]()->SimProcess {
			simu(valid(master.a)) = '0';
			co_await OnClk(clk);

			for (size_t i = 6; i < 10; i++) // from 64B to 1kB
			{
				//make request
				simu(master.a->address) = 0;
				simu(master.a->opcode) = (size_t) scl::TileLinkA::OpCode::Get;
				simu(master.a->param) = 0;
				simu(master.a->size) = i;
				simu(valid(master.a)) = '1';

				co_await scl::strm::performTransferWait(master.a, clk);
				simu(valid(master.a)) = '0';

				size_t expectedBeats = 8 * (1 << i) / 512;
				//get response and check
				for (size_t j = 0; j < expectedBeats; j++)
				{
					co_await scl::strm::performTransferWait(*master.d, clk);
					BOOST_TEST(host.memory().read(j * 512, 512) == (sim::DefaultBitVectorState) simu((*master.d)->data));
				}
			}
			co_await OnClk(clk);
			stopTest();
		}
	);

	if (false) recordVCD("dut.vcd");
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(pci_requester_512bit_cheapBurst_chopped_test, BoostUnitTestSimulationFixture)
{
	Area area{ "top_area", true };
	using namespace scl::pci;
	const Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t memSizeInBytes = 1 << 10; // 1kB space

	scl::sim::PcieHostModel host(scl::sim::RandomBlockDefinition{ 0, memSizeInBytes * 8, 1039834});
	host.defaultHandlers();
	host.updateHandler(TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(64, 5));
	addSimulationProcess([&, this]()->SimProcess { return host.completeRequests(clk, 0, 50); });

	scl::TileLinkUB master = makePciMasterCheapBurst(host.requesterInterface(512_b), {}, 4_b);

	pinIn(master, "link");

	addSimulationProcess([&, this]()->SimProcess { return scl::strm::readyDriverRNG(*master.d, clk, 50); });

	addSimulationProcess(
		[&, this]()->SimProcess {
			simu(valid(master.a)) = '0';
			co_await OnClk(clk);

			for (size_t i = 6; i < 10; i++) // from 64B to 1kB
			{
				//make request
				simu(master.a->address) = 0;
				simu(master.a->opcode) = (size_t) scl::TileLinkA::OpCode::Get;
				simu(master.a->param) = 0;
				simu(master.a->size) = i;
				simu(valid(master.a)) = '1';

				co_await scl::strm::performTransferWait(master.a, clk);
				simu(valid(master.a)) = '0';

				//get response and check
				size_t expectedBeats = 8 * (1 << i) / 512;
				for (size_t j = 0; j < expectedBeats; j++)
				{
					co_await scl::strm::performTransferWait(*master.d, clk);
					BOOST_TEST(host.memory().read(j * 512, 512) == (sim::DefaultBitVectorState) simu((*master.d)->data));
					BOOST_TEST(simu((*master.d)->size) == i);
					BOOST_TEST(simu((*master.d)->opcode) == (size_t) scl::TileLinkD::AccessAckData);
					BOOST_TEST(simu((*master.d)->error) == '0');
				}
			}
			co_await OnClk(clk);
			stopTest();
		}
	);

	if (false) recordVCD("dut.vcd");
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 5, 1'000'000 }));
}




BOOST_FIXTURE_TEST_CASE(dispatcher_test, BoostUnitTestSimulationFixture) {
	using namespace scl::pci;
	const Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	BitWidth tlpStreamW = 128_b;
	size_t numberOfPackets = 128;

	TlpPacketStream<scl::EmptyBits> in(tlpStreamW);
	emptyBits(in) = BitWidth::count(in->width().bits());
	pinIn(in, "in");

	scl::StreamDemux disp = pciDispatcher(move(in));

	auto out0 = disp.out(0);
	pinOut(out0, "out0");
	auto out1 = disp.out(1);
	pinOut(out1, "out1");


	std::mt19937 rng(0xBADC0DE);
	std::vector<scl::sim::TlpInstruction> instTo0;
	std::vector<scl::sim::TlpInstruction> instTo1;

	size_t packetsReceivedBy0 = 0;
	size_t packetsReceivedBy1 = 0;
	//send completion packets through in
	addSimulationProcess([&]()->SimProcess {
		uint32_t rand;
		for (size_t i = 0; i < numberOfPackets; i++){
			if(i % 32 == 0) rand = rng();

			scl::sim::TlpInstruction inst;
			inst.opcode = TlpOpcode::completionWithData;
			inst.length = 10;
			inst.completerID = 0;
			inst.lowerByteAddress = 0;
			inst.byteCount = 0;
			inst.payload.emplace();
			for (size_t i = 0; i < inst.length; i++)
				inst.payload->emplace_back(rng());
			
			if (rand & 1) {
				inst.tag = 1;
				instTo1.emplace_back(inst);
			}
			else {
				inst.tag = 0;
				instTo0.emplace_back(inst);
			}
			rand >>= 1;
			co_await scl::strm::sendPacket(in, scl::strm::SimPacket(inst), clk);
		}
		while ((packetsReceivedBy0 + packetsReceivedBy1) < numberOfPackets)
			co_await OnClk(clk);
		BOOST_TEST(packetsReceivedBy0 == instTo0.size());
		BOOST_TEST(packetsReceivedBy1 == instTo1.size());
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
	});

	//receive packets through out0
	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(out0, clk, 50));
		while (true) {
			scl::strm::SimPacket packet = co_await scl::strm::receivePacket(out0, clk);
			auto reconstructedTlp = scl::sim::TlpInstruction::createFrom(packet.payload);
			BOOST_TEST(reconstructedTlp == instTo0[packetsReceivedBy0]);
			packetsReceivedBy0++;
		}
	});

	//receive packets through out1
	addSimulationProcess([&]()->SimProcess {
		fork(scl::strm::readyDriverRNG(out1, clk, 50));
		while (true) {
			auto packet = co_await scl::strm::receivePacket(out1, clk);
			auto reconstructedTlp = scl::sim::TlpInstruction::createFrom(packet.payload);
			BOOST_TEST(reconstructedTlp == instTo1[packetsReceivedBy1]);
			packetsReceivedBy1++;
		}
	});

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}

struct pciInterfaceSplitterFixture : BoostUnitTestSimulationFixture {
	bool sendRequesterRequests = false;
	bool sendCompleterRequests = false;
	bool sendCompleterCompletions = false;
	bool sendRequesterCompletions = false;
	size_t numPackets = 10;
	std::mt19937 rng;

	void runTest() {
		rng.seed(1140);
		Clock clk({ .absoluteFrequency = 100'000'000 });
		ClockScope clkScp(clk);

		BitWidth tlpW = 128_b;

		scl::pci::TlpPacketStream<scl::EmptyBits, scl::pci::BarInfo> rx(tlpW);
		emptyBits(rx) = BitWidth::count(tlpW.bits());
		pinIn(rx, "rx");

		scl::pci::CompleterInterface compInt;
		*compInt.completion = tlpW;
		emptyBits(compInt.completion) = BitWidth::count(tlpW.bits());
		pinIn(compInt.completion, "compComp");

		scl::pci::RequesterInterface reqInt;
		**reqInt.request = tlpW;
		emptyBits(*reqInt.request) = BitWidth::count(tlpW.bits());
		pinIn(*reqInt.request, "reqReq");

		scl::Stream tx = scl::pci::interfaceSplitter(std::move(compInt), std::move(reqInt), move(rx));

		pinOut(reqInt.completion, "reqComp");
		pinOut(compInt.request, "compReq");
		pinOut(tx, "tx");
			
		addSimulationProcess([&, this]()->SimProcess {
			fork(scl::strm::readyDriverRNG(tx, clk, 50));
			fork(scl::strm::readyDriverRNG(compInt.request, clk, 50));
			fork(scl::strm::readyDriverRNG(reqInt.completion, clk, 50));
			co_await OnClk(clk);
		});

		std::vector<sim::DefaultBitVectorState> compReq;
		if (sendCompleterRequests) {
			compReq.resize(numPackets);
			for (auto& dbv : compReq) {
				scl::sim::TlpInstruction inst = scl::sim::TlpInstruction::randomizeNaive(rng() & 1 ?
					scl::pci::TlpOpcode::memoryReadRequest64bit :
					scl::pci::TlpOpcode::memoryWriteRequest64bit,
					rng(),
					true
				);
				dbv = inst;
			}
		}
		
		scl::SimulationSequencer rxSeq;
		//sending completion requests
		addSimulationProcess([&, this]()->SimProcess {
			if(!sendRequesterCompletions && !sendCompleterRequests)
				scl::strm::simuStreamInvalidate(rx);
			for (const auto& dbv : compReq) {
				fork(scl::strm::sendPacket(rx, scl::strm::SimPacket(dbv), clk, rxSeq));
				co_await OnClk(clk);
			}
		});

		std::vector<sim::DefaultBitVectorState> reqComp;
		if (sendRequesterCompletions) {
			reqComp.resize(numPackets);
			for (auto& dbv : reqComp) {
				scl::sim::TlpInstruction inst = scl::sim::TlpInstruction::randomizeNaive(scl::pci::TlpOpcode::completionWithData, rng(), true);
				dbv = inst;
			}
		}
		//sending requester completion
		addSimulationProcess([&, this]()->SimProcess {
			if(!sendRequesterCompletions && !sendCompleterRequests)
				scl::strm::simuStreamInvalidate(rx);
			for (const auto& dbv : reqComp) {
				fork(scl::strm::sendPacket(rx, scl::strm::SimPacket(dbv), clk, rxSeq));
				co_await OnClk(clk);
			}
		});

		std::vector<sim::DefaultBitVectorState> compComp;
		if (sendCompleterCompletions) {
			compComp.resize(numPackets);
			for (auto& dbv : compComp) {
				scl::sim::TlpInstruction inst = scl::sim::TlpInstruction::randomizeNaive(scl::pci::TlpOpcode::completionWithData, rng(), true);
				dbv = inst;
			}
		}
		//sending completer completion
		addSimulationProcess([&, this]()->SimProcess {
			if(!sendCompleterCompletions)
				scl::strm::simuStreamInvalidate(compInt.completion);
			for (const auto& dbv : compComp)
				co_await scl::strm::sendPacket(compInt.completion, scl::strm::SimPacket(dbv), clk);
			co_await OnClk(clk);
		});

		std::vector<sim::DefaultBitVectorState> reqReq;
		if (sendRequesterRequests) {
			reqReq.resize(numPackets);
			for (auto& dbv : reqReq) {
				scl::sim::TlpInstruction inst = scl::sim::TlpInstruction::randomizeNaive(rng() & 1 ?
					scl::pci::TlpOpcode::memoryReadRequest64bit :
					scl::pci::TlpOpcode::memoryWriteRequest64bit,
					rng(),
					true
				);
				dbv = inst;
			}
		}
		//sending requester request
		addSimulationProcess([&, this]()->SimProcess {
			if(!sendRequesterRequests)
				scl::strm::simuStreamInvalidate(*reqInt.request);
			for (const auto& dbv : reqReq)
				co_await scl::strm::sendPacket(*reqInt.request, scl::strm::SimPacket(dbv), clk);
			co_await OnClk(clk);
		});

		size_t done = 0;

		if (sendCompleterRequests) {
			addSimulationProcess([&, this]()->SimProcess {
				for (const auto& dbv : compReq) {
					auto receivedPacket = co_await scl::strm::receivePacket(compInt.request, clk);
					BOOST_TEST(dbv == receivedPacket.payload);
				}
				done++;
				});
		}

		if (sendRequesterCompletions) {
			addSimulationProcess([&, this]()->SimProcess {
				for (const auto& dbv : reqComp) {
					auto receivedPacket = co_await scl::strm::receivePacket(reqInt.completion, clk);
					BOOST_TEST(dbv == receivedPacket.payload);
				}
				done++;
				});
		}

		
		if (sendCompleterCompletions) {
			addSimulationProcess([&, this]()->SimProcess {
				for (const auto& dbv : compComp) {
					bool packetIsCompleterCompletion = false;
					scl::strm::SimPacket receivedPacket;
					while (!packetIsCompleterCompletion) {
						receivedPacket = co_await scl::strm::receivePacket(tx, clk);
						if (scl::sim::TlpInstruction::createFrom(receivedPacket.payload).opcode == scl::pci::TlpOpcode::completionWithData)
							packetIsCompleterCompletion = true;
					}
					BOOST_TEST(dbv == receivedPacket.payload);
				}
				done++;
			});
		}

		if (sendRequesterRequests) {
			addSimulationProcess([&, this]()->SimProcess {
				for (const auto& dbv : reqReq) {
					bool packetIsRequesterRequest = false;
					scl::strm::SimPacket receivedPacket;
					while (!packetIsRequesterRequest) {
						receivedPacket = co_await scl::strm::receivePacket(tx, clk);
						if (scl::sim::TlpInstruction::createFrom(receivedPacket.payload).opcode != scl::pci::TlpOpcode::completionWithData)
							packetIsRequesterRequest = true;
					}
					BOOST_TEST(dbv == receivedPacket.payload);
				}
				done++;
			});
		}

		size_t total = 0;
		if (sendCompleterRequests) total++;
		if (sendCompleterCompletions) total++;
		if (sendRequesterRequests) total++;
		if (sendRequesterCompletions) total++;

		addSimulationProcess([&, this]()->SimProcess {
			while (done < total)
				co_await OnClk(clk);
			stopTest();
		});

		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(pci_splitter_req_req, pciInterfaceSplitterFixture)
{
	sendRequesterRequests = true;
	//sendCompleterRequests = false;
	//sendCompleterCompletions = false;
	//sendRequesterCompletions = false;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_comp_comp, pciInterfaceSplitterFixture)
{
	//sendRequesterRequests = false;
	//sendCompleterRequests = false;
	sendCompleterCompletions = true;
	//sendRequesterCompletions = false;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_full_tx, pciInterfaceSplitterFixture)
{
	sendRequesterRequests = true;
	//sendCompleterRequests = false;
	sendCompleterCompletions = true;
	//sendRequesterCompletions = false;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_comp_req, pciInterfaceSplitterFixture)
{
	//sendRequesterRequests = false;
	sendCompleterRequests = true;
	//sendCompleterCompletions = false;
	//sendRequesterCompletions = false;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_req_comp, pciInterfaceSplitterFixture)
{
	//sendRequesterRequests = false;
	//sendCompleterRequests = false;
	//sendCompleterCompletions = false;
	sendRequesterCompletions = true;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_full_rx, pciInterfaceSplitterFixture)
{
	//sendRequesterRequests = false;
	sendCompleterRequests = true;
	//sendCompleterCompletions = false;
	sendRequesterCompletions = true;
	//numPackets = 10;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(pci_splitter_full_rx_tx, pciInterfaceSplitterFixture)
{

	sendRequesterRequests = true;
	sendCompleterRequests = true;
	sendCompleterCompletions = true;
	sendRequesterCompletions = true;
	//numPackets = 10;
	runTest();
}




