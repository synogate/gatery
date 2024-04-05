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
#include "SimPci.h"

namespace gtry::scl::sim {
	TlpInstruction::operator gtry::sim::DefaultBitVectorState()
	{
		return this->asDefaultBitVectorState();
	}

	gtry::sim::DefaultBitVectorState TlpInstruction::asDefaultBitVectorState(bool headerOnly)
	{
		gtry::sim::DefaultBitVectorState packet;
		packet.resize(32);
		DefaultBitVectorWriter helper(packet);

		helper
			.write(this->opcode, 8_b)
			.write(this->th, 1_b)
			.skip(1_b)
			.write(this->idBasedOrderingAttr2, 1_b)
			.skip(1_b) 
			.write(this->tc, 3_b)
			.skip(1_b)
			.write(*this->length >> 8, 2_b)
			.write(this->at, 2_b)
			.write(this->noSnoopAttr0, 1_b)
			.write(this->relaxedOrderingAttr1, 1_b)
			.write(this->ep, 1_b)
			.write(this->td, 1_b)
			.write(*this->length & 0xFF, 8_b);

		HCL_DESIGNCHECK(helper.offset == 32);

		switch (this->opcode) {
			case scl::pci::TlpOpcode::memoryReadRequest64bit: {
				packet.resize(128);
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
				HCL_DESIGNCHECK_HINT(this->requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(this->tag, "tag not set");
				HCL_DESIGNCHECK_HINT(this->wordAddress, "address not set");
				helper
					.write(this->requesterID >> 8, 8_b)
					.write(this->requesterID & 0xFF, 8_b)
					.write(this->tag, 8_b)
					.write(this->firstDWByteEnable, 4_b)
					.write(this->lastDWByteEnable, 4_b)
					.write((*this->wordAddress >> (56-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (48-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (40-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (32-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (24-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (16-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (8-2)) & 0xFF, 8_b)
					.write(this->ph, 2_b)
					.write((*this->wordAddress) & 0b00111111, 6_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case scl::pci::TlpOpcode::memoryWriteRequest64bit:
			{
				packet.resize(128);
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
				HCL_DESIGNCHECK_HINT(this->requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(this->tag, "tag not set");
				HCL_DESIGNCHECK_HINT(this->wordAddress, "address not set");
				HCL_DESIGNCHECK_HINT(this->payload, "you forgot to set the payload");
				helper
					.write(this->requesterID >> 8, 8_b)
					.write(this->requesterID & 0xFF, 8_b)
					.write(this->tag, 8_b)
					.write(this->firstDWByteEnable, 4_b)
					.write(this->lastDWByteEnable, 4_b)
					.write((*this->wordAddress >> (56-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (48-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (40-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (32-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (24-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (16-2)) & 0xFF, 8_b)
					.write((*this->wordAddress >> (8-2)) & 0xFF, 8_b)
					.write(this->ph, 2_b)
					.write((*this->wordAddress) & 0b00111111, 6_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			//intentional fall-through behavior:
			case scl::pci::TlpOpcode::completionWithData:
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
			case scl::pci::TlpOpcode::completionWithoutData:
			{
				packet.resize(96);
				HCL_DESIGNCHECK_HINT(this->completerID, "completer id not set");
				if (this->completionStatus == CompletionStatus::unsupportedRequest) {
					helper
						.write(*this->completerID >> 8, 8_b)
						.write(*this->completerID & 0xFF, 8_b)
						.skip(4_b)
						.write(this->byteCountModifier, 1_b)
						.write(this->completionStatus, 3_b)
						.skip(8_b)
						.write(this->requesterID >> 8, 8_b)
						.write(this->requesterID & 0xFF, 8_b)
						.write(this->tag, 8_b)
						.skip(8_b);
				}
				else {
					HCL_DESIGNCHECK_HINT(this->byteCount, "byteCount not set");
					HCL_DESIGNCHECK_HINT(this->lowerByteAddress, "address (lower address) not set, this corresponds to the byte address of the payload in the current TLP");
					helper
						.write(*this->completerID >> 8, 8_b)
						.write(*this->completerID & 0xFF, 8_b)
						.write(*this->byteCount >> 8, 4_b)
						.write(this->byteCountModifier, 1_b)
						.write(this->completionStatus, 3_b)
						.write(*this->byteCount & 0xFF, 8_b)
						.write(this->requesterID >> 8, 8_b)
						.write(this->requesterID & 0xFF, 8_b)
						.write(this->tag, 8_b)
						.write(*this->lowerByteAddress, 7_b)
						.skip(1_b);
				}

				HCL_DESIGNCHECK_HINT(helper.offset == 96, "incomplete header");
				break;
			}
			default:
				HCL_DESIGNCHECK_HINT(false, std::string(magic_enum::enum_name(this->opcode)) + "is not implemented");
				break;
		}

		if (this->payload && !headerOnly) {
			packet.resize(packet.size() + *this->length * 32);
			for (size_t i = 0; i < *this->length; i++)
				helper.write((*this->payload)[i], 32_b);
		}

		return packet;
	}

	/**
	 * @brief   fills the opcode with the desired opcode and every other field with completely random bits. 
	 *			Does not check for coherence or create actual valid tlps.
	 *			Intended for testing purposes only.
	 * @param op opcode
	 * @param seed seed for the randomization
	 * @return naively random instruction
	*/
	TlpInstruction TlpInstruction::randomizeNaive(pci::TlpOpcode op, size_t seed, bool addCoherentPayload) {
		std::mt19937 rng{ (unsigned int) seed };
		TlpInstruction ret;

		ret.opcode = op;
		ret.th = (rng() & 0x1);		
		ret.idBasedOrderingAttr2 = (rng() & 0x1);
		ret.tc = (rng() & 0b111);				
		ret.length  = (rng() & 0x3FF);			
		ret.at = (rng() & 0x3);						
		ret.noSnoopAttr0 = (rng() & 1);	
		ret.relaxedOrderingAttr1 = (rng() & 1);
		ret.ep = (rng() & 1);						
		ret.td = (rng() & 1);					
		ret.ph = (rng() & 3);
		ret.requesterID = (rng() & 0xFFFF);
		ret.tag = (rng() & 0xFFFF);
		ret.firstDWByteEnable = (rng() & 0xF);
		ret.lastDWByteEnable = (rng() & 0xF);

		uint64_t address = rng();
		address <<= 32;
		address |= rng();

		ret.wordAddress = address & 0x3FFF'FFFF'FFFF'FFFFull;
		ret.lowerByteAddress = rng() & 0x7F;
		ret.completerID = (rng() & 0xFFFF);
		ret.completionStatus = (CompletionStatus) (rng() & 0x7);
		ret.byteCountModifier =  (rng() & 1);
		ret.byteCount = (rng() & 0xFFF);

		if(addCoherentPayload)
			if (ret.opcode == TlpOpcode::memoryWriteRequest64bit || ret.opcode == TlpOpcode::completionWithData)
			{
				ret.payload.emplace();
				ret.payload->reserve(*ret.length);
				for (size_t i = 0; i < *ret.length; i++)
					ret.payload->push_back(rng());
			}

		return ret;
	}

	TlpInstruction TlpInstruction::createFrom(const gtry::sim::DefaultBitVectorState& raw){
		TlpInstruction inst;
		inst.th = readState(raw, 8, 1);
		inst.idBasedOrderingAttr2 = readState(raw, 10, 1);
		inst.tc = (uint8_t) readState(raw, 12, 3);
		inst.length = (readState(raw, 16, 2) << 8) & 0x3FF;
		inst.at = readState(raw, 18, 2);
		inst.noSnoopAttr0 = readState(raw, 20, 1);
		inst.relaxedOrderingAttr1 = readState(raw, 21, 1);
		inst.ep = readState(raw, 22, 1);
		inst.td = readState(raw, 23, 1);
		*inst.length |= readState(raw, 24, 8) & 0xFF;

		inst.opcode = (scl::pci::TlpOpcode) readState(raw, 0, 8);
		bool isRW;
		bool hasPayload;
		size_t hdrSize;

		switch(inst.opcode){
		case scl::pci::TlpOpcode::memoryReadRequest64bit:
			isRW = true;
			hasPayload = false;
			hdrSize = 128;
			break;
		case scl::pci::TlpOpcode::memoryWriteRequest64bit:
			isRW = true;
			hasPayload = true;
			hdrSize = 128;
			break;
		case scl::pci::TlpOpcode::completionWithData:
			isRW = false;
			hasPayload = true;
			hdrSize = 96;
			break;
		case scl::pci::TlpOpcode::completionWithoutData:
			isRW = false;
			hasPayload = false;
			hdrSize = 96;
			break;
		}

		if (isRW) {
			inst.requesterID = readState(raw, 32, 8) << 8;
			inst.requesterID |= readState(raw, 40, 8) & 0xFF;
			inst.tag = (uint8_t) readState(raw, 48, 8);
			inst.firstDWByteEnable = readState(raw, 56, 4);
			inst.lastDWByteEnable  = readState(raw, 60, 4);
			inst.wordAddress = readState(raw, 64, 8) << 56;
			*inst.wordAddress |= readState(raw, 72, 8) << 48;
			*inst.wordAddress |= readState(raw, 80, 8) << 40;
			*inst.wordAddress |= readState(raw, 88, 8) << 32;
			*inst.wordAddress |= readState(raw, 96, 8) << 24;
			*inst.wordAddress |= readState(raw, 104, 8) << 16;
			*inst.wordAddress |= readState(raw, 112, 8) << 8;
			*inst.wordAddress |= readState(raw, 122, 6) << 2;
			*inst.wordAddress >>= 2; //
			inst.ph = (uint8_t) readState(raw, 120, 2);
		}
		else {
			inst.completionStatus = (CompletionStatus) readState(raw, 53, 3);
			if (inst.completionStatus == CompletionStatus::successfulCompletion) {
				inst.byteCount = readState(raw, 48, 4) << 8;
				inst.byteCountModifier = readState(raw, 52, 1);
				*inst.byteCount |= readState(raw, 56, 8);
				inst.lowerByteAddress = (uint8_t) readState(raw, 88, 7);
			}
			inst.completerID = readState(raw, 32, 8) << 8;
			*inst.completerID |= readState(raw, 40, 8) & 0xFF;
			inst.requesterID = readState(raw, 64, 8) << 8;
			inst.requesterID |= readState(raw, 72, 8) & 0xFF;
			inst.tag = (uint8_t) readState(raw, 80, 8);
		}
		if (hasPayload) {
			HCL_DESIGNCHECK_HINT(raw.size() % 32 == 0, "payload is not an integer number of DW. Severe Problem!");
			size_t payloadDw = (raw.size() - hdrSize) / 32;
			inst.payload.emplace();
			inst.payload->reserve(payloadDw);
			for (size_t i = 0; i < payloadDw; i++)
				inst.payload->push_back((uint32_t) readState(raw, hdrSize + i * 32, 32));
		}
		return inst;
	}

	uint64_t readState(gtry::sim::DefaultBitVectorState raw, size_t offset, size_t size) {
		HCL_DESIGNCHECK_HINT(gtry::sim::allDefined<gtry::sim::DefaultConfig>(raw, offset, size), "the extracted bit vector is not fully defined");
		return raw.extract(gtry::sim::DefaultConfig::VALUE, offset, size);
	}

	std::ostream& operator << (std::ostream& s, const TlpInstruction& inst) {
		HCL_DESIGNCHECK_HINT(false, "not yet implemented, put normal brackets around your test");
	}


	template<typename T>
	DefaultBitVectorWriter& DefaultBitVectorWriter::write(const T& value, BitWidth size) {
		HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "not enough space in destination to write value");
		destination.insert(gtry::sim::DefaultConfig::VALUE, offset, size.value, (gtry::sim::DefaultConfig::BaseType)value);
		destination.setRange(gtry::sim::DefaultConfig::DEFINED, offset, size.value);
		offset += size.value;
		return *this;
	}
	DefaultBitVectorWriter& DefaultBitVectorWriter::skip(BitWidth size) {
		HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "offset would overflow after this operation. This is not allowed");
		offset += size.value;
		return *this;
	}
}
