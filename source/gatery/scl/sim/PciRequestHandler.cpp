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
#include <gatery/hlim/postprocessing/MemoryStorage.h>
#include <gatery/scl/stream/SimuHelpers.h>
#include "PciRequestHandler.h"

namespace gtry::scl::sim {

	SimProcess PciRequestHandler::respond(const TlpInstruction& request, const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq)
	{
		TlpInstruction completion = request;
		completion.opcode = TlpOpcode::completionWithoutData;
		completion.completionStatus = CompletionStatus::unsupportedRequest;
		completion.completerID = 0xFFFF;
		strm::SimPacket completionPacket(completion.asDefaultBitVectorState(true));
		co_await strm::sendPacket(responseStream, completionPacket, clk, sendingSeq);
	}

	SimProcess Completer::respond(const TlpInstruction& request,const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq)
	{
		TlpInstruction completion = request;
		if(request.opcode == TlpOpcode::memoryReadRequest64bit)
		{
			completion.opcode = TlpOpcode::completionWithData;
			completion.lowerByteAddress = (uint8_t) (*request.wordAddress * 4); //initial lower byte address
			completion.completerID = m_completerId;
			completion.byteCount = *request.length * 4; // initial byte count left
			completion.completionStatus = CompletionStatus::successfulCompletion;
			
			strm::SimPacket completionPacket(completion.asDefaultBitVectorState(true));
		
			size_t baseBitAddress = *request.wordAddress * 32;
			for (size_t dword = 0; dword < request.length; dword++)
			{
				completionPacket << mem.read(baseBitAddress + dword * 32, 32);
				
			}
			co_await strm::sendPacket(responseStream, completionPacket, clk, sendingSeq);
		} 
		else 
			HCL_DESIGNCHECK_HINT(false, "This completer does not have an implementation for the opcode: " + std::string(magic_enum::enum_name(request.opcode)) + ". You have incorrectly bound this opcode with a request handler that does not support it.");
	}

	SimProcess CompleterInChunks::respond(const TlpInstruction& request,const hlim::MemoryStorage& mem, const TlpPacketStream<EmptyBits>& responseStream, const Clock& clk, SimulationSequencer& sendingSeq)
	{
		TlpInstruction completion = request;
		if(request.opcode == TlpOpcode::memoryReadRequest64bit)
		{
			completion.opcode = TlpOpcode::completionWithData;
			completion.lowerByteAddress = (uint8_t) (*request.wordAddress * 4); //initial lower byte address
			completion.completerID = m_completerId;
			size_t payloadSizeInBytes = *request.length * 4; //can be adapted to include first_be and last_be
			size_t bytesLeft = payloadSizeInBytes;
			completion.byteCount = bytesLeft;
			completion.completionStatus = CompletionStatus::successfulCompletion;

			strm::SimPacket completionPacket(completion.asDefaultBitVectorState(true));

			size_t baseBitAddress = *request.wordAddress * 32;
			size_t numPackets = (payloadSizeInBytes + m_chunkSizeInBytes - 1) / m_chunkSizeInBytes;
			for (size_t i = 0; i < numPackets; i++)
			{
				size_t sentBytes = 0;
				for (size_t byte = 0; byte < bytesLeft && byte < m_chunkSizeInBytes; byte++)
				{
					completionPacket << mem.read(baseBitAddress, 8);
					baseBitAddress += 8;
					sentBytes++;
				}
				bytesLeft -= sentBytes;

				for (size_t j = 0; j < m_gapInCyclesBetweenChunksOfSameRequest; j++) co_await OnClk(clk);
				
				co_await strm::sendPacket(responseStream, completionPacket, clk, sendingSeq);

				completion.byteCount = bytesLeft;
				*completion.lowerByteAddress = (uint8_t) (baseBitAddress >> 3);
				completionPacket = completion.asDefaultBitVectorState(true);
			}
		} 
		else 
			HCL_DESIGNCHECK_HINT(false, "This completer does not have an implementation for the opcode: " + std::string(magic_enum::enum_name(request.opcode)) + ". You have incorrectly bound this opcode with a request handler that does not support it.");
	}

}
