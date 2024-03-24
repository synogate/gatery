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
#include <gatery/scl/sim/SimPci.h> 
#include <gatery/scl/arch/xilinx/XilinxPci.h> 

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(pcie_axi4_vendorUnlocking, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	using namespace scl::pci::xilinx;
	Clock clk({ .absoluteFrequency = 250'000'000 });
	ClockScope clkScp(clk);

	Axi4PacketStream<xilinx::CQUser> inAxi(512_b);
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


	TlpPacketStream<scl::EmptyBits, BarInfo> outTlp = completerRequestVendorUnlocking(move(inAxi));
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
			simu(inAxi.get<xilinx::CQUser>().first_be) = 0x0000FFFF;
			simu(inAxi.get<xilinx::CQUser>().last_be)  = 0x00000000;
			simu(inAxi.get<xilinx::CQUser>().tph_present) = '0';

			co_await WaitFor(Seconds{ 0,1 });

			BOOST_TEST(simu(valid(outTlp)) == '0');
			BOOST_TEST(simu(eop(outTlp)) == '0');
			BOOST_TEST(simu(ready(inAxi)) == '0');

			auto dbv = (sim::DefaultBitVectorState) simu(*outTlp);
			//TlpInstruction instr = TlpInstruction::createFrom(dbv);
			co_await OnClk(clk);

			stopTest();
		});


	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(pcie_axi4_vendorUnlocking_inv, BoostUnitTestSimulationFixture) {

	using namespace scl::pci;
	using namespace scl::sim;
	using namespace scl::pci::xilinx;
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
	inst.lowerByteAddress = 0x7F;
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



	xilinx::Axi4PacketStream<xilinx::CCUser> out = xilinx::completerCompletionVendorUnlocking(move(in));
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


	if (false) { recordVCD("dut.vcd"); }
	design.postprocess();
	//design.visualize("dut");

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}
