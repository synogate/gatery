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
#include <gatery/scl_pch.h>
#include "PcieHostModel.h"
#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/sim/SimulationSequencer.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <external/magic_enum.hpp>


namespace gtry::scl::sim {
	using namespace gtry;

	PcieHostModel::PcieHostModel(std::optional<RandomBlockDefinition> randomBlockDefinition, uint64_t memorySizeInBytes)
	{
		m_memSize = 8 * memorySizeInBytes;
		if (randomBlockDefinition)
			m_memInit = hlim::MemoryStorage::Initialization::setAllDefinedRandom(
				randomBlockDefinition->size,
				randomBlockDefinition->offset,
				randomBlockDefinition->seed);
	}

	PcieHostModel& PcieHostModel::defaultHandlers()
	{
		updateHandler(TlpOpcode::memoryReadRequest64bit, std::make_unique<Completer>());
		return *this;
	}

	PcieHostModel& PcieHostModel::updateHandler(TlpOpcode op, std::unique_ptr<PciRequestHandler> requestHandler)
	{
		m_requestHandlers[op] = std::move(requestHandler);
		return *this;
	}

	PcieHostModel& PcieHostModel::updateHandler(std::map<TlpOpcode, std::unique_ptr<PciRequestHandler>> requestHandlers)
	{
		for (auto& binding : requestHandlers) {
			m_requestHandlers[binding.first] = move(binding.second);
		}
		return *this;
	}

	void PcieHostModel::requesterRequest(TlpPacketStream<EmptyBits>&& rr)
	{ 
		m_rr.emplace(move(rr)); 
		pinOut(*m_rr, "host_rr", {.simulationOnlyPin = true});
	}

	TlpPacketStream<EmptyBits>& PcieHostModel::requesterCompletion() { 
		m_rc.emplace((*m_rr)->width()); 
		emptyBits(*m_rc) =  BitWidth::count((*m_rc)->width().bits());
		pinIn(*m_rc, "host_rc", {.simulationOnlyPin = true});
		return *m_rc; 
	}
	
	RequesterInterface PcieHostModel::requesterInterface(BitWidth tlpW) {
		m_rr.emplace(tlpW);
		emptyBits(*m_rr) =  BitWidth::count((*m_rr)->width().bits());
		pinOut(*m_rr, "host_rr", {.simulationOnlyPin = true});

		m_rc.emplace(tlpW);
		emptyBits(*m_rc) =  BitWidth::count((*m_rc)->width().bits());
		pinIn(*m_rc, "host_rc", {.simulationOnlyPin = true});

		scl::pci::RequesterInterface reqInt{
			.request = { constructFrom(*m_rr) },
			.completion = constructFrom(*m_rc),
		};

		*m_rr <<= *reqInt.request;
		reqInt.completion <<= *m_rc;
		
		return reqInt;
	}

	PciRequestHandler* PcieHostModel::handler(TlpOpcode op)
	{
		auto it = m_requestHandlers.find(op);
		if (it != m_requestHandlers.end())
			return it->second.get();
		else
			return &m_defaultHandler;
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
			BOOST_TEST((m_requestHandlers.find(tlp.opcode) != m_requestHandlers.end()), "the opcode (ftm and type): " << magic_enum::enum_name(tlp.opcode) << "is not supported");
		}
	}

	

	SimProcess PcieHostModel::completeRequests(const Clock& clk, size_t delay, std::optional<uint8_t> rngReadyPercentage)
	{
		auto &mem = emplaceSimData<hlim::MemoryStorageSparse>(memLabel, m_memSize, m_memInit);

		HCL_DESIGNCHECK_HINT(m_rr, "requesterRequest port is not connected");
		HCL_DESIGNCHECK_HINT(m_rc, "requesterCompletion port is not connected");
		simu(valid(*m_rc)) = '0';
		SimulationSequencer sendingSeq;

		if(rngReadyPercentage)
			fork(scl::strm::readyDriverRNG(*m_rr, clk, *rngReadyPercentage));
		else
			fork(scl::strm::readyDriver(*m_rr, clk));

		while (true) {
			auto simPacket = co_await gtry::scl::strm::receivePacket(*m_rr, clk);
			//simPacket must be captured by copy because it might get overwritten with the next request before the first response has even gotten out
			fork([&, simPacket, this]()->SimProcess {
				for (size_t i = 0; i < delay; i++)
					co_await OnClk(clk);
				TlpInstruction request = TlpInstruction::createFrom(simPacket.payload);
				co_await this->handler(request.opcode)->respond(request, mem, *m_rc, clk, sendingSeq);
			});
		}
	}

	hlim::MemoryStorage& PcieHostModel::memory() {
		return getSimData<hlim::MemoryStorageSparse>(memLabel);
	}
}
