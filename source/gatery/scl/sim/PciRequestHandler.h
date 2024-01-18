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
#include <gatery/scl/io/pci/pci.h>
#include <gatery/scl/sim/SimulationSequencer.h>
#include <gatery/scl/sim/SimPci.h>
#include <gatery/hlim/postprocessing/MemoryStorage.h>

namespace gtry::scl::sim {
	using namespace scl::pci;

	//must respond to everything with unsupported request
	class PciRequestHandler {
	public:
		PciRequestHandler() = default;
		PciRequestHandler(uint16_t completerId) : m_completerId(completerId) {}
		virtual SimProcess respond(const TlpInstruction& request, const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq);
	protected:
		uint16_t m_completerId = 0x5678;
	};
	
	class Completer : public PciRequestHandler {
	public:
		Completer() = default;
		Completer(uint16_t completerId) : PciRequestHandler(completerId) {}
		virtual	SimProcess respond(const TlpInstruction& request, const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq) override;

	};

	class CompleterInChunks : public PciRequestHandler {
	public:
		//CompleterInChunks() = default;
		CompleterInChunks(size_t chunkSizeInBytes = 64, size_t gapInCyclesBetweenChunksOfSameRequest = 0) : 
			m_chunkSizeInBytes(chunkSizeInBytes), 
			m_gapInCyclesBetweenChunksOfSameRequest(gapInCyclesBetweenChunksOfSameRequest){}
		virtual	SimProcess respond(const TlpInstruction& request, const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq) override;
	private:
		size_t m_chunkSizeInBytes = 64;
		size_t m_gapInCyclesBetweenChunksOfSameRequest = 0;
	};

	class Unsupported : public PciRequestHandler {
	public:
		
	};

}
