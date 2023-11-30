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

#include <gatery/scl/io/pci.h>
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

	std::vector<uint8_t> data(memSizeInBytes);
	for (size_t i = 0; i < memSizeInBytes; i++)
		data[i] = (uint8_t) i;

	hlim::MemoryStorageDense mem(memSizeInBytes * 8, hlim::MemoryStorageDense::Initialization{ .background = data });
	scl::sim::PcieHostModel host(mem);
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
		scl::strm::SimPacket responsePacket = co_await scl::strm::receivePacket(requesterCompletion, clk);
		auto tlp = scl::sim::TlpInstruction::createFrom(responsePacket.payload);
		BOOST_TEST((tlp.opcode == pci::TlpOpcode::completionWithData));
		BOOST_TEST(tlp.payload->front() == 0x1234);
		BOOST_TEST(*tlp.byteCount == 4);
		BOOST_TEST(tlp.completionStatus == (size_t)pci::CompletionStatus::successfulCompletion);
		stopTest();
	});

	

	recordVCD("dut.vcd");
	design.postprocess();

	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}





