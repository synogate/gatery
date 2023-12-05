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
#pragma once

#include <gatery/frontend.h>
#include <gatery/scl/io/pci.h>
#include <gatery/scl/sim/SimPci.h>
#include <gatery/hlim/postprocessing/MemoryStorage.h>
#include "PciRequestHandler.h"


namespace gtry::scl::sim {
	using namespace scl::pci;

	class PcieHostModel 
	{
	public:
		PcieHostModel(uint64_t memorySizeInBytes = 64, bool memInitRandomDefined = true, uint32_t seed = 1259);
		PcieHostModel(std::unique_ptr<hlim::MemoryStorage> mem) : m_mem(move(mem)) {}

		PcieHostModel& defaultHandlers();

		PcieHostModel& updateHandler(TlpOpcode op, std::unique_ptr<PciRequestHandler> requestHandler);
		PcieHostModel& updateHandler(std::map<TlpOpcode, std::unique_ptr<PciRequestHandler>> requestHandlers);
		
		void requesterRequest(TlpPacketStream<EmptyBits>&& rr);
		TlpPacketStream<EmptyBits>& requesterCompletion();

		PciRequestHandler* handler(TlpOpcode);

		hlim::MemoryStorage& memory() { return *m_mem; }

		SimProcess assertInvalidTlp(const Clock& clk);
		SimProcess assertPayloadSizeDoesntMatchHeader(const Clock& clk);
		SimProcess assertUnsupportedTlp(const Clock& clk);
		SimProcess completeRequests(const Clock& clk, size_t delay = 0);

	private:
		std::unique_ptr<hlim::MemoryStorage> m_mem;
		std::optional<TlpPacketStream<EmptyBits>> m_rr;
		std::optional<TlpPacketStream<EmptyBits>> m_rc;
		std::map<TlpOpcode, std::unique_ptr<PciRequestHandler>> m_requestHandlers;
		Unsupported m_defaultHandler;

	};
}
