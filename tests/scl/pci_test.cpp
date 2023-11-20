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

#include <gatery/scl/io/pci.h>

using namespace boost::unit_test;
using namespace gtry;



BOOST_FIXTURE_TEST_CASE(tlp_builder_test, BoostUnitTestSimulationFixture)
{
	using namespace scl::pci;
	//Clock clk = Clock({ .absoluteFrequency = 100'000'000 });
	//ClockScope clkScope(clk);
	TlpInstruction read{
		.opcode = TlpOpcode::memoryReadRequest64bit,
		.lastDWByteEnable = 0,
		.address = 0x0123456789ABCDEC,
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
		.address = 0x0123456789ABCDEC,
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

scl::strm::SimPacket writeWord(size_t address, uint32_t data) {
	scl::pci::TlpInstruction write{
		.opcode = scl::pci::TlpOpcode::memoryWriteRequest64bit,
		.length = 1,
		.requesterID = 0xABCD,
		.tag = 0xFF,
		.lastDWByteEnable = 0,
		.address = address,
	};
	write.payload = std::vector<uint32_t>{ data };
	return scl::strm::SimPacket(write);
}

scl::strm::SimPacket readWord(size_t address) {
	scl::pci::TlpInstruction read{
		.opcode = scl::pci::TlpOpcode::memoryReadRequest64bit,
		.length = 1,
		.requesterID = 0xABCD,
		.tag = 0xFF,
		.lastDWByteEnable = 0,
		.address = address,
	};
	return scl::strm::SimPacket(read);
}
BOOST_FIXTURE_TEST_CASE(tlp_to_tilelink_rw64_1dw, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);

	TlpPacketStream<scl::EmptyBits> in(512_b);
	in.set(scl::EmptyBits{ BitWidth::count(512) });
	pinIn(in, "in");

	BitWidth tlAddrW = 5_b;
	BitWidth tlDataW = 32_b;
	BitWidth tlSourceW = pack(tlpAnswerInfo{}).width();

	Memory<BVec> mem(tlAddrW.count(), BVec(tlDataW));
	scl::TileLinkUL ul = scl::tileLinkInit<scl::TileLinkUL>(tlAddrW, tlDataW, tlSourceW);
	mem <<= ul;

	HCL_NAMED(ul);

	auto completerInterface = makeTileLinkMaster(move(ul), in->width());
	completerInterface.request <<= in;

	TlpPacketStream<scl::EmptyBits> out = constructFrom(in);
	out <<= completerInterface.completion;
	pinOut(out, "out");

	setName((*out)(96, 32_b), "out_payload");
	setName((*out)(0, 96_b), "out_hdr");

	scl::SimulationSequencer sendSeq;
	addSimulationProcess([&, this]()->SimProcess{
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, writeWord(0, 0xAAAAAAAA), clk, sendSeq); });
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, writeWord(4, 0xBBBBBBBB), clk, sendSeq); });
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, writeWord(8, 0xCCCCCCCC), clk, sendSeq); });
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, readWord(0), clk, sendSeq); });
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, readWord(4), clk, sendSeq); });
		fork([&, this]()->SimProcess { return scl::strm::sendPacket(in, readWord(8), clk, sendSeq); });
		co_await OnClk(clk);
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


BOOST_FIXTURE_TEST_CASE(pcie_axi4_vendorUnlocking, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	Clock clk({ .absoluteFrequency = 250'000'000 });
	ClockScope clkScp(clk);

	amd::axi4PacketStream<amd::CQUser> inAxi(512_b);
	inAxi.get<scl::Keep>() = { 16_b };
	pinIn(inAxi, "inAxi");

	BVec inAxi_low = 64_b;
	pinIn(inAxi_low, "inAxi_low");

	BVec inAxi_high = 64_b;
	pinIn(inAxi_high, "inAxi_high");

	BVec inAxi_payload = 32_b;
	pinIn(inAxi_payload, "inAxi_payload");

	inAxi->lower(64_b) = inAxi_low;
	(*inAxi)(64, 64_b) = inAxi_high;
	(*inAxi)(128, 32_b) = inAxi_payload;


	TlpPacketStream<scl::EmptyBits> outTlp = amd::completerRequestVendorUnlocking(move(inAxi));
	pinOut(outTlp, "outTlp");

	setName((*outTlp)(0,32_b), "dw0");
	setName((*outTlp)(1*32,32_b), "dw1");
	setName((*outTlp)(2*32,32_b), "dw2");
	setName((*outTlp)(3*32,32_b), "dw3");
	setName((*outTlp)(4*32,32_b), "dw4");
	setName((*outTlp)(5*32,32_b), "dw5");

	addSimulationProcess(
		[&, this]() -> SimProcess {
			simu(valid(inAxi)) = '0';
			simu(eop(inAxi)) = '0';
			simu(ready(outTlp)) = '0';
			simu(keep(inAxi)) = 0x001F;

			simu(inAxi_low) = 0xA123456789ABCDECull;
			simu(inAxi_high) = 0x00ab00bbddee0001ull;
			simu(inAxi_payload) = 0xFFFFFFFFull;
			simu(inAxi.get<amd::CQUser>().first_be) = 0x0000FFFF;
			simu(inAxi.get<amd::CQUser>().last_be)  = 0x00000000;
			simu(inAxi.get<amd::CQUser>().tph_present) = '0';

			co_await WaitFor(Seconds{ 0,1 });

			BOOST_TEST(simu(valid(outTlp)) == '0');
			BOOST_TEST(simu(eop(outTlp)) == '0');
			BOOST_TEST(simu(ready(inAxi)) == '0');

			auto dbv = (sim::DefaultBitVectorState) simu(*outTlp);
			//TlpInstruction instr = TlpInstruction::createFrom(dbv);
			co_await OnClk(clk);

		
			stopTest();
		});


	if (true) { recordVCD("dut.vcd"); }
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(pcie_axi4_vendorUnlocking_inv, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	Clock clk({ .absoluteFrequency = 250'000'000 });
	ClockScope clkScp(clk);

	TlpInstruction inst{};
	inst.opcode = TlpOpcode::completionWithData;
	inst.length = 1;
	inst.completerID = 0xAABB;
	inst.completionStatus = (size_t) CompletionStatus::successfulCompletion;
	inst.byteCount = 4;
	inst.requesterID = 0xCCDD;
	inst.tag = 0xEE;
	inst.address = 0x7F;
	inst.payload = std::vector({ 0xFFFFFFFF });

	TlpPacketStream<scl::EmptyBits> in(512_b);
	emptyBits(in) = BitWidth::count(in->width().bits());
	pinIn(in, "in");

	setName((*in)(0,32_b), "dw0");
	setName((*in)(1*32,32_b), "dw1");
	setName((*in)(2*32,32_b), "dw2");
	setName((*in)(3*32,32_b), "dw3");
	setName((*in)(4*32,32_b), "dw4");
	setName((*in)(5*32,32_b), "dw5");



	amd::axi4PacketStream<amd::CCUser> out = amd::completerCompletionVendorUnlocking(move(in));
	pinOut(out, "out");
	setName((*out)(0,32_b), "dw0_out");
	setName((*out)(1*32,32_b), "dw1_out");
	setName((*out)(2*32,32_b), "dw2_out");
	setName((*out)(3*32,32_b), "dw3_out");
	setName((*out)(4*32,32_b), "dw4_out");
	setName((*out)(5*32,32_b), "dw5_out");

	addSimulationProcess(
		[&, this]() -> SimProcess {
			return scl::strm::sendPacket(in, scl::strm::SimPacket(inst), clk);
		});


	if (true) { recordVCD("dut.vcd"); }
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}
