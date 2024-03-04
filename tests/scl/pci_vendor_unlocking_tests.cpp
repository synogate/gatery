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
	
	scl::strm::RvPacketStream rxStream(move(ptileInstance.rx()));
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
		//scl::strm::SimPacket packet(dbvs);
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
			co_await performTransferWait(in, clk);
			simu(ptileHeader).invalidate();
			simu(valid(in)) = '0';
			simu(eop(in)).invalidate();
			//for (size_t i = 0; i < 2; i++)
			//	co_await OnClk(clk);
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
			co_await performPacketTransferWait(in, clk);
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

	PTile ptileInstance(PTile::Presets::Gen3x16_256());
	ptileInstance.connectNInitDone(IntelResetIP{}.ninit_done());

	Clock clk = ptileInstance.userClock();
	ClockScope clkScp(clk);

	BitWidth addW = 4_b;
	BitWidth dataW = 32_b;

	Memory<BVec> mem(addW.count(), dataW);
	TileLinkUL tl = tileLinkInit<TileLinkUL>(addW, dataW, pack(TlpAnswerInfo{}).width());
	mem <<= tl;

	CompleterInterface complInt = pci::makeTileLinkMaster(move(tl), ptileInstance.settings().dataBusW);

	complInt.request = ptileRxVendorUnlocking(ptileInstance.rx())
		.template remove<PTileBarRange>()
		.add(BarInfo{ .id = ConstBVec(0, 3_b), .logByteAperture = ConstUInt(0, 6_b)});
	auto something = ptileTxVendorUnlocking(move(complInt.completion));
	ptileInstance.tx(move(something));

	design.postprocess();

	if (false) {
		m_vhdlExport.emplace("export/ptile/top.vhd");
		m_vhdlExport->targetSynthesisTool(new gtry::IntelQuartus());
		//m_vhdlExport->writeStandAloneProjectFile("export_thing.qsf");
		//m_vhdlExport->writeConstraintsFile("top_constraints.sdc");
		//m_vhdlExport->writeClocksFile("top_clocks.sdc");
		(*m_vhdlExport)(design.getCircuit());
		//outputVHDL("dut.vhd");
	}
}

