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

	std::cout << "READ" << std::endl;
	for (size_t byte = 0; byte < dbv.size() / 8; byte++) {
		if (byte % 4 == 0) std::cout << std::endl;
		std::cout << std::setw(10) << std::dec << dbv.extract(byte * 8, 8) << " " << std::setw(2) << std::hex << dbv.extract(byte * 8, 8) << std::endl;
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
	std::cout << "WRITE" << std::endl;
	for (size_t byte = 0; byte < dbv.size() / 8; byte++) {
		if (byte % 4 == 0) std::cout << std::endl;
		std::cout << std::setw(8) << std::dec << dbv.extract(byte * 8, 8) << " " << std::setw(2) << std::hex << dbv.extract(byte * 8, 8) << std::endl;
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

	TlpInstruction read = TlpInstruction::randomize(TlpOpcode::memoryReadRequest64bit, std::random_device{}() );

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

	TlpInstruction comp = TlpInstruction::randomize(TlpOpcode::completionWithData,  std::random_device{}());

	BVec rawHeader = 96_b;
	pinIn(rawHeader, "in_raw");

	CompletionHeader compHdr = CompletionHeader::fromRaw(rawHeader);

	pinOut(compHdr, "out");

	BVec reconstructed = (BVec)compHdr;
	
	CompletionHeader compHdrRecon = CompletionHeader::fromRaw(reconstructed);
	
	pinOut(compHdrRecon, "out");


	addSimulationProcess(
		[&, this]()->SimProcess {
			co_await OnClk(clk);
			simu(rawHeader) = (sim::DefaultBitVectorState) comp;
			co_await WaitFor(Seconds{ 0, 1});
			BOOST_TEST(simu(compHdr.common.type) == ((size_t) comp.opcode & 0x1Fu));
			BOOST_TEST(simu(compHdr.common.fmt) == ((size_t) comp.opcode >> 5));
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
			BOOST_TEST(simu(compHdr.byteCount) == *comp.byteCount);
			BOOST_TEST(simu(compHdr.byteCountModifier) == comp.byteCountModifier);
			BOOST_TEST(simu(compHdr.lowerByteAddress) == *comp.lowerByteAddress);
			BOOST_TEST(simu(compHdr.completionStatus) == (size_t) comp.completionStatus);

			BOOST_TEST(simu(compHdrRecon.common.type) == simu(compHdr.common.type));
			BOOST_TEST(simu(compHdrRecon.common.fmt)  == simu(compHdr.common.fmt));
			BOOST_TEST(simu(compHdrRecon.common.addressType)  == simu(compHdr.common.addressType));
			BOOST_TEST(simu(compHdrRecon.common.processingHintPresence)  == simu(compHdr.common.processingHintPresence));
			BOOST_TEST(simu(compHdrRecon.common.attributes.idBasedOrdering)  == simu(compHdr.common.attributes.idBasedOrdering));
			BOOST_TEST(simu(compHdrRecon.common.attributes.noSnoop)  == simu(compHdr.common.attributes.noSnoop));
			BOOST_TEST(simu(compHdrRecon.common.attributes.relaxedOrdering)  == simu(compHdr.common.attributes.relaxedOrdering));
			BOOST_TEST(simu(compHdrRecon.common.digest)  == simu(compHdr.common.digest));
			BOOST_TEST(simu(compHdrRecon.common.poisoned)  == simu(compHdr.common.poisoned));
			BOOST_TEST(simu(compHdrRecon.common.length)  == simu(compHdr.common.length));
			BOOST_TEST(simu(compHdrRecon.common.trafficClass)  == simu(compHdr.common.trafficClass));
			
			BOOST_TEST(simu(compHdrRecon.requesterId) == simu(compHdr.requesterId));
			BOOST_TEST(simu(compHdrRecon.tag) == simu(compHdr.tag));
			BOOST_TEST(simu(compHdrRecon.completerId) == simu(compHdr.completerId));
			BOOST_TEST(simu(compHdrRecon.byteCount) == simu(compHdr.byteCount));
			BOOST_TEST(simu(compHdrRecon.byteCountModifier) == simu(compHdr.byteCountModifier));
			BOOST_TEST(simu(compHdrRecon.lowerByteAddress) == simu(compHdr.lowerByteAddress));
			BOOST_TEST(simu(compHdrRecon.completionStatus) == simu(compHdr.completionStatus));

			co_await OnClk(clk);
			stopTest();
		}
	);

	design.postprocess();

	BOOST_TEST(!runHitsTimeout({1,1'000'000}));
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


	if (true) { recordVCD("dut.vcd"); }
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
			fork([&, this]()->SimProcess { return scl::strm::sendPacket(rc, reqCompPacket, clk, seq); });
		}

		for (size_t i = 0; i < iterations; i++)
		{
			co_await scl::strm::performTransferWait(d, clk);
			BOOST_TEST(simu(d->source) == reqComp.tag);
			BOOST_TEST(simu(d->size) == 6);
			auto sigHandle = simu(d->data);
			sim::DefaultBitVectorState dbv = sigHandle.eval();
			for (size_t j = 0; j < 512 / 32; j++)
			{
				uint32_t mask = dbv.extractNonStraddling(sim::DefaultConfig::DEFINED, j * 32, 32);
				BOOST_TEST(mask == (uint32_t)((1ull << 32) - 1));
				uint32_t word = dbv.extractNonStraddling(sim::DefaultConfig::VALUE, j * 32, 32);
				BOOST_TEST(reqComp.payload->at(j) == word);
			}
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

	recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(pci_requester_512bit_tilelink_to_pcieHostModel_test, BoostUnitTestSimulationFixture)
{
	Area area{ "top_area", true };
	using namespace scl::pci;
	const Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	const size_t memSizeInBytes = 64;

	scl::sim::PcieHostModel host(memSizeInBytes);
	host.defaultHandlers();
	host.completeRequests(clk);
	
	scl::TileLinkUL master = makePciMasterFullW(host.requesterInterface(512_b));

	scl::TileLinkMasterModel linkModel;

	scl::TileLinkUL fromSimulation = constructFrom((scl::TileLinkUL&)linkModel.getLink());
	fromSimulation <<= (scl::TileLinkUL&)linkModel.getLink();

	HCL_NAMED(fromSimulation);

	{
		Area area("cpuBusSimulationOverride", true);

		setName(cpuPortMathClk, "cpuPortMathClk_before");
		setName(fromSimulation, "fromSimulation_before");

		completerPort.a <<= simOverrideDownstream<scl::TileLinkChannelA>(move(cpuPortMathClk.a), move(fromSimulation.a));

		auto [newD, newSimulationD] = simOverrideUpstream<scl::TileLinkChannelD>(move(*completerPort.d));
		*cpuPortMathClk.d <<= newD;
		*fromSimulation.d <<= newSimulationD;

		setName(fromSimulation, "fromSimulation_after");
		setName(cpuPortMathClk, "cpuPortMathClk_after");
	}

	auto memoryMapEntries = gtry::scl::exportAddressSpaceDescription(mMap.getTree().physicalDescription);

	std::ofstream file("test_export/pcie_requester_test.gtkwave.filter", std::fstream::binary);
	gtry::scl::writeGTKWaveFilterFile(file, memoryMapEntries);

	scl::driver::SimulationFiberMapped32BitTileLink driverInterface(linkModel, (Clock&) clk);

	struct pcie_requester_test_with_driver { };
	using Map = scl::driver::DynamicMemoryMap<pcie_requester_test_with_driver>;
	Map::memoryMap = scl::driver::MemoryMap(memoryMapEntries);
	Map driverMemoryMap;

	addSimulationFiber([&memoryMapEntries, &driverInterface, &clk, &driverMemoryMap, this](){
		std::vector<std::byte> recalledData(32);

		gtry::scl::driver::readFromTileLink(driverInterface, driverMemoryMap.template get<"requester_tilelink">(), 0xABCD'DEAD'BEEF'0000ull, std::as_writable_bytes(std::span(recalledData)));

		stopTest();
		}
	);

	board.attachTileLinkSlaveToCompleterPort(0, move(cpuPortMathClk));

	std::ofstream mmapFile("test_export/memoryMappedRequesterPort.h", std::fstream::binary);
	gtry::scl::format(mmapFile, "pcie4c_requester_test", memoryMapEntries);

	recordVCD("test_export/memoryMappedRequesterPort.vcd");
	design.postprocess();

	if (true) {
		m_vhdlExport.emplace("test_export/memoryMappedRequesterPort.vhd");
		m_vhdlExport->targetSynthesisTool(new gtry::XilinxVivado());
		//m_vhdlExport->writeStandAloneProjectFile("export_thing.xpr");
		//m_vhdlExport->writeConstraintsFile("top_constraints.sdc");
		m_vhdlExport->writeClocksFile("top_clocks.sdc");
		(*m_vhdlExport)(design.getCircuit());
		//outputVHDL("pcie4c.vhd");
	}

	//BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}
