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

#include <gatery/scl/synthesisTools/IntelQuartus.h>
#include <gatery/scl/arch/intel/IntelDevice.h>

#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/io/pci/PciToTileLink.h>
#include <gatery/scl/sim/SimPci.h>
#include <gatery/scl/arch/intel/PTile.h>
#include <gatery/scl/arch/intel/IntelResetIP.h>

#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/tilelink/tilelink.h>

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(ptile_vhdl_test, BoostUnitTestSimulationFixture)
{
	auto device = std::make_unique<scl::IntelDevice>();
	device->setupDevice("AGFB014R24B2E2V");
	design.setTargetTechnology(std::move(device));
	using namespace scl::arch::intel;
	
	PTile ptileInstance(PTile::Presets::Gen3x16_256());
	
	IntelResetIP rstInstance;
	
	ptileInstance.connectNInitDone(rstInstance.ninit_done());
	
	Clock clk = ptileInstance.userClock();
	ClockScope clkScp(clk);
	
	scl::strm::RvPacketStream<BVec, scl::Error, PTileHeader, PTilePrefix> txStream(256_b);
	
	valid(txStream) = '0';
	ptileInstance.tx(move(txStream));
	
	auto rxStream(move(ptileInstance.rx()));
	ready(rxStream) = '1';
	
	design.postprocess();
	
	if (false) {
		m_vhdlExport.emplace("dut_project/top.vhd");
		m_vhdlExport->targetSynthesisTool(new gtry::IntelQuartus());
		//m_vhdlExport->writeStandAloneProjectFile("export_thing.qsf");
		//m_vhdlExport->writeConstraintsFile("top_constraints.sdc");
		//m_vhdlExport->writeClocksFile("top_clocks.sdc");
		(*m_vhdlExport)(design.getCircuit());
		//outputVHDL("dut.vhd");
	}
}

BOOST_FIXTURE_TEST_CASE(ptile_rx_vendor_unlocking_only_read_requests, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);
	using namespace scl::arch::intel;

	BitWidth dataW = 256_b;
	size_t nReads = 100;

	RvPacketStream<BVec, EmptyBits, PTileHeader, PTilePrefix, PTileBarRange> in(dataW);
	emptyBits(in) = BitWidth::count(in->width().bits());
	pinIn(in, "in");

	BVec ptileHeader = 128_b;
	pinIn(ptileHeader, "inputHeader" );
	in.template get<PTileHeader>().header = swapEndian(ptileHeader);
	
	TlpPacketStream<EmptyBits, PTileBarRange> out = ptileRxVendorUnlocking(move(in));
	pinOut(out, "out");

	addSimulationProcess([&, this]()->SimProcess { return strm::readyDriverRNG(out, clk, 50); });

	auto makeReadHeader = []() {
		std::mt19937_64	rng(20225);
		scl::sim::TlpInstruction readReq;
		readReq.opcode = TlpOpcode::memoryReadRequest64bit;
		readReq.length = rng() & 0xF;
		readReq.wordAddress = rng() >> 2;
		return readReq.asDefaultBitVectorState();
	};

	std::vector<gtry::sim::DefaultBitVectorState> reads(nReads);
	for (size_t i = 0; i < nReads; i++)
		reads[i] = makeReadHeader();

	
	//send reads
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < nReads; i++)
		{
			simu(ptileHeader) = reads[i];
			simu(valid(in)) = '1';
			simu(eop(in)) = '1';
			//simu(emptyBits(in)) = 0; doesn't matter because it's header only
			co_await performTransferWait(in, clk);
			simu(ptileHeader).invalidate();
			simu(valid(in)) = '0';
			simu(eop(in)).invalidate();
		}
	});


	//receive and decode gatery tlp
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < nReads; i++)
		{
			strm::SimPacket tlp = co_await strm::receivePacket(out, clk);
			BOOST_TEST(tlp.payload == reads[i]);
		}
		co_await OnClk(clk);
		stopTest();
	});
	
	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}



BOOST_FIXTURE_TEST_CASE(ptile_rx_vendor_unlocking_only_write_requests, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);
	using namespace scl::arch::intel;

	BitWidth dataW = 256_b;
	size_t nWrites = 1000;

	RvPacketStream<BVec, EmptyBits, PTileHeader, PTilePrefix, PTileBarRange> in(dataW);
	emptyBits(in) = BitWidth::count(in->width().bits());
	pinIn(in, "in");

	BVec ptileHeader = 128_b;
	pinIn(ptileHeader, "inputHeader" );
	in.template get<PTileHeader>().header = swapEndian(ptileHeader);

	TlpPacketStream<EmptyBits, PTileBarRange> out = ptileRxVendorUnlocking(move(in));
	pinOut(out, "out");

	addSimulationProcess([&, this]()->SimProcess { return strm::readyDriverRNG(out, clk, 50); });

	std::mt19937_64	rng(21225);
	auto makeWritePacket = [&rng]() {
		scl::sim::TlpInstruction readReq;
		readReq.opcode = TlpOpcode::memoryWriteRequest64bit;
		size_t length = (rng() & 0xF) + 1;
		readReq.length = length;
		readReq.wordAddress = rng() >> 2;
		readReq.payload.emplace(*readReq.length);
		for (size_t i = 0; i < *readReq.length; i++)
			(*readReq.payload)[i] = (uint32_t)rng();
		return readReq.asDefaultBitVectorState();
		};

	std::vector<gtry::sim::DefaultBitVectorState> writePackets(nWrites);
	for (size_t i = 0; i < nWrites; i++) {
		writePackets[i] = makeWritePacket();
	}
	

	//send write data
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < nWrites; i++) {
			size_t something = writePackets[i].size();
			auto dbv = writePackets[i].extract(128, writePackets[i].size() - 128);
			co_await strm::sendPacket(in, strm::SimPacket(dbv), clk);
			size_t wait = (rng() & 3);
			for (size_t i = 0; i < wait; i++)
				co_await OnClk(clk);
		}
	});

	//send write header
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < nWrites; i++)
		{
			simu(ptileHeader) = writePackets[i].extract(0, 128);
			do { 
				co_await performTransferWait(in, clk);
				simu(ptileHeader).invalidate();
			} while (!(simuReady(in) == '1' && simuValid(in) == '1' && simuEop(in) == '1'));
		}
		co_await OnClk(clk);
	});

	//receive and decode gatery tlp
	addSimulationProcess([&, this]()->SimProcess {
		for (size_t i = 0; i < nWrites; i++)
		{
			strm::SimPacket tlp = co_await strm::receivePacket(out, clk);
			BOOST_TEST(tlp.payload == writePackets[i], "packet " + std::to_string(i) + " failed");
			//BOOST_TEST(tlp.payload == writePackets[i]);
		}
		co_await OnClk(clk);
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 100, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(ptile_hail_mary_completer, BoostUnitTestSimulationFixture)
{
	auto device = std::make_unique<scl::IntelDevice>();
	device->setupDevice("AGFB014R24B2E2V");
	design.setTargetTechnology(std::move(device));
	using namespace scl::arch::intel;
	//using namespace scl::pci;

	PTile ptileInstance(PTile::Presets::Gen3x16_256());
	ptileInstance.connectNInitDone(IntelResetIP{}.ninit_done());

	Clock clk = ptileInstance.userClock();
	ClockScope clkScp(clk);

	scl::Counter blinky(29_b);
	blinky.inc();

	pinOut(blinky.value().upper(4_b), "fm6_led");

	BitWidth addW = 8_b;
	BitWidth dataW = 32_b;

	Memory<BVec> mem(addW.count(), dataW);
	mem.initZero();
	TileLinkUL tl = tileLinkInit<TileLinkUL>(addW, dataW, pack(TlpAnswerInfo{}).width());
	mem <<= tl;

	CompleterInterface complInt = pci::makeTileLinkMaster(move(tl), ptileInstance.settings().dataBusW);

	RvPacketStream<BVec, EmptyBits, PTileHeader, PTilePrefix, PTileBarRange> rxSim(ptileInstance.settings().dataBusW);
	emptyBits(rxSim) = BitWidth::count(rxSim->width().bits());
	pinIn(rxSim, "rxSim", { .simulationOnlyPin = true });

	BVec intelHeader = 128_b;
	pinIn(intelHeader, "intel_header", { .simulationOnlyPin = true });
	rxSim.get<PTileHeader>() = simOverride(rxSim.get<PTileHeader>(), PTileHeader{ swapEndian(intelHeader) });

	auto rxUnlocked = ptileRxVendorUnlocking(simOverrideDownstream(ptileInstance.rx(), move(rxSim)) | strm::regDownstream())
		.template remove<PTileBarRange>()
		| strm::attach(BarInfo{ .id = ConstBVec(0, 3_b), .logByteAperture = ConstUInt(20, 6_b)});//log byte aperture should be set to ip value
	HCL_NAMED(rxUnlocked);
	complInt.request <<= move(rxUnlocked);

	auto [txLocked, txSim] = simOverrideUpstream(ptileTxVendorUnlocking(move(complInt.completion)));
	HCL_NAMED(txLocked);
	ptileInstance.tx(move(txLocked).template remove<EmptyBits>());
	pinOut(txSim, "txSim", { .simulationOnlyPin = true });


	scl::sim::TlpInstruction readInst;
	readInst.opcode = TlpOpcode::memoryReadRequest64bit;
	readInst.length = 1;
	readInst.lastDWByteEnable = 0;
	readInst.wordAddress = 5;

	scl::sim::TlpInstruction writeInst;
	writeInst.opcode = TlpOpcode::memoryWriteRequest64bit;
	writeInst.length = 1;
	writeInst.lastDWByteEnable = 0;
	writeInst.wordAddress = 5;
	writeInst.payload = std::vector<uint32_t>({ 42 });

	//addSimulationProcess([&, this]()->SimProcess { return strm::readyDriver(txSim, clk); });
	addSimulationProcess([&, this]()->SimProcess { simu(ready(txSim)) = '1'; co_await OnClk(clk); });

	//send read, write, read
	addSimulationProcess([&, this]()->SimProcess {
		simu(intelHeader) = readInst;
		simu(valid(rxSim)) = '0';
		simu(eop(rxSim)).invalidate();

		//very very important, the intel core does not assert high until it sees ready

		do {
			co_await OnClk(clk);
		} while (simu(ready(rxSim)) != '1');
		simu(intelHeader) = readInst;
		simu(valid(rxSim)) = '1';
		simu(eop(rxSim)) = '1';
		co_await strm::performPacketTransferWait(rxSim, clk);
		simu(intelHeader).invalidate();
		simuStreamInvalidate(rxSim);
		for (size_t i = 0; i < 20; i++)
			co_await OnClk(clk);

		simu(intelHeader) = gtry::sim::DefaultBitVectorState(writeInst).extract(0, 128);
		auto payload = gtry::sim::DefaultBitVectorState(writeInst).extract(128, 32);
		payload.resize(ptileInstance.settings().dataBusW.bits());
		simu(*rxSim) = payload;
		simu(emptyBits(rxSim)) = ptileInstance.settings().dataBusW.bits() - 32;
		simu(valid(rxSim)) = '1';
		simu(eop(rxSim)) = '1';
		co_await strm::performPacketTransferWait(rxSim, clk);
		simu(intelHeader).invalidate();
		simuStreamInvalidate(rxSim);
		for (size_t i = 0; i < 20; i++)
			co_await OnClk(clk);

		simu(intelHeader) = readInst;
		simu(valid(rxSim)) = '1';
		simu(eop(rxSim)) = '1';
		co_await strm::performPacketTransferWait(rxSim, clk);
		simu(intelHeader).invalidate();
		simuStreamInvalidate(rxSim);
		for (size_t i = 0; i < 20; i++)
			co_await OnClk(clk);

		stopTest();
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));

	if (true) {
		m_vhdlExport.emplace("export/ptile/top.vhd");
		m_vhdlExport->targetSynthesisTool(new gtry::IntelQuartus());
		m_vhdlExport->writeStandAloneProjectFile("completer.qsf");
		m_vhdlExport->writeConstraintsFile("completer_constraints.sdc");
		m_vhdlExport->writeClocksFile("completer.sdc");
		(*m_vhdlExport)(design.getCircuit());
		//outputVHDL("dut.vhd");
	}
}


BOOST_FIXTURE_TEST_CASE(ptile_tx_vendor_unlocking_completion_only, BoostUnitTestSimulationFixture)
{
	Clock clk({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clk);
	using namespace scl::arch::intel;

	BitWidth dataW = 256_b;
	size_t nTlps = 10;

	TlpPacketStream<EmptyBits> in(dataW);
	emptyBits(in) = BitWidth::count(in->width().bits());
	pinIn(in, "in");

	scl::strm::RvPacketStream<BVec, EmptyBits, scl::Error, PTileHeader, PTilePrefix> out = ptileTxVendorUnlocking(move(in));
	pinOut(out, "out");

	BVec header = swapEndian(out.template get<PTileHeader>().header);
	pinOut(header, "header");

	addSimulationProcess([&, this]()->SimProcess { return strm::readyDriverRNG(out, clk, 50); });

	std::mt19937_64	rng(21225);
	auto makeCompletionTlp = [&rng]()->gtry::sim::DefaultBitVectorState{
		scl::sim::TlpInstruction compReq;
		compReq.opcode = TlpOpcode::completionWithData;
		compReq.length = (rng() & 0x3) + 1;
		compReq.payload.emplace(*compReq.length);
		compReq.lowerByteAddress = 0x7F;
		compReq.completerID = 0x4567;
		compReq.completionStatus = CompletionStatus::successfulCompletion;
		compReq.byteCountModifier = 0;
		compReq.byteCount = 40;
		for (size_t i = 0; i < *compReq.length; i++)
			(*compReq.payload)[i] = (uint32_t)rng();
		return compReq.asDefaultBitVectorState();
		};

	std::vector<gtry::sim::DefaultBitVectorState> completionPackets(nTlps);
	for (auto& entry : completionPackets) {
		entry = makeCompletionTlp();
	}

	//send tlp's
	addSimulationProcess([&, this]()->SimProcess {
		for (auto& tlp : completionPackets) {
			co_await strm::sendPacket(in, strm::SimPacket(tlp), clk);
			size_t wait = 5;//(rng() & 3);
			for (size_t i = 0; i < wait; i++)
				co_await OnClk(clk);
		}
	});

	//receive and decode payload
	addSimulationProcess([&, this]()->SimProcess {
		for (auto& tlp : completionPackets) {
			strm::SimPacket payloadReceived = co_await strm::receivePacket(out, clk);
			BOOST_TEST(payloadReceived.payload == tlp.extract(96, tlp.size() - 96));
		}
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		co_await OnClk(clk);
		stopTest();
		});

	//receive and decode header
	addSimulationProcess([&, this]()->SimProcess {
		for (auto& tlp : completionPackets) {
			do {
				co_await OnClk(clk);
			} while (!(simuValid(out) == '1' && simuReady(out) == '1' && simuSop(out) == '1'));
			auto rawExtendedHeader = tlp.extract(0, 96);
			rawExtendedHeader.resize(128);
			rawExtendedHeader.setRange(gtry::sim::DefaultConfig::Plane::DEFINED, 96, 32);
			BOOST_TEST(simu(header) == rawExtendedHeader);
		}
		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

//BOOST_FIXTURE_TEST_CASE(testReproCase, gtry::BoostUnitTestSimulationFixture)
//{
//	using namespace gtry;
//
//	{
//		UInt data = pinIn(16_b).setName("data");
//		UInt select = pinIn(4_b).setName("select");
//		Bit enable = pinIn().setName("enable");
//
//		UInt mask = ConstUInt(0, 32_b);
//		mask(select, 16_b) |= '1';
//		UInt output = data;
//		IF (enable)
//			output &= mask.lower(16_b);
//
//		pinOut(output).setName("output");
//	}
//
//	//design.visualize("before");
//	//
//	//testCompilation();
//	//
//	//design.visualize("after");
//}
