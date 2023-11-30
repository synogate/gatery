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
#include <gatery/pch.h>
#include "PcieHostModel.h"
#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/sim/SimulationSequencer.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <external/magic_enum.hpp>


namespace gtry::scl::sim {
	using namespace gtry;

	void PcieHostModel::requesterRequest(TlpPacketStream<EmptyBits>&& rr) 
	{ 
		m_rr.emplace(move(rr)); 
		pinOut(*m_rr, "host_rr");
	}

	TlpPacketStream<EmptyBits>& PcieHostModel::requesterCompletion() { 
		m_rc.emplace((*m_rr)->width()); 
		emptyBits(*m_rc) =  BitWidth::count((*m_rc)->width().bits());
		pinIn(*m_rc, "host_rc");
		return *m_rc; 
	}

	SimProcess PcieHostModel::assertInvalidTlp(const Clock& clk)
	{
		fork(this->assertPayloadSizeDoesntMatchHeader(clk));
		while (true) co_await OnClk(clk);
	}

	SimProcess PcieHostModel::assertPayloadSizeDoesntMatchHeader(const Clock& clk)
	{
		HCL_DESIGNCHECK_HINT(m_rr, "requesterRequest port is not connected");
		while (true) {
			auto simPacket = co_await gtry::scl::strm::receivePacket(*m_rr, clk);
			auto tlp = TlpInstruction::createFrom(simPacket.payload);
			BOOST_TEST(tlp.payload->size() == *tlp.length);
		}
	}

	SimProcess PcieHostModel::assertUnsupportedTlp(const Clock& clk)
	{
		HCL_DESIGNCHECK_HINT(m_rr, "requesterRequest port is not connected");
		while (true) {
			auto simPacket = co_await gtry::scl::strm::receivePacket(*m_rr, clk);
			auto tlp = TlpInstruction::createFrom(simPacket.payload);
			BOOST_TEST((m_opcodesSupported.contains(tlp.opcode)), "the opcode (ftm and type): 0x" << magic_enum::enum_name(tlp.opcode) << "is not supported");
		}
	}

	SimProcess PcieHostModel::completeRequests(const Clock& clk, size_t delay)
	{
		HCL_DESIGNCHECK_HINT(m_rr, "requesterRequest port is not connected");
		HCL_DESIGNCHECK_HINT(m_rc, "requesterCompletion port is not connected");
		SimulationSequencer sendingSeq;
		fork([&, this]()->SimProcess { return scl::strm::readyDriver(*m_rr, clk); });
		while (true) {
			auto simPacket = co_await gtry::scl::strm::receivePacket(*m_rr, clk);


			//simPacket must be captured by copy because it might get overwritten with the next request before the first response has even gotten out
			fork([&, simPacket, this]()->SimProcess {
				for (size_t i = 0; i < delay; i++)
					co_await OnClk(clk);
				TlpInstruction request = TlpInstruction::createFrom(simPacket.payload);
				if(request.opcode == TlpOpcode::memoryReadRequest64bit)
				{
					TlpInstruction completion = request;
					completion.opcode = TlpOpcode::completionWithData;
					completion.lowerByteAddress = (uint8_t) (*request.wordAddress << 2); //initial lower byte address
					completion.completerID = 0x5678;
					completion.byteCount = *request.length << 2; // initial byte count left

					strm::SimPacket completionPacket(completion.asDefaultBitVectorState(true));

					size_t baseBitAddress = *request.wordAddress << 5;
					for (size_t dword = 0; dword < request.length; dword++)
					{
						completionPacket << m_mem.read(baseBitAddress, 32);
						baseBitAddress += 32;
					}

					co_await strm::sendPacket(*m_rc, completionPacket, clk, sendingSeq);
				}
			});
		}
	}
}
