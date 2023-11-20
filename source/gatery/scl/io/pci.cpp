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
#include "gatery/pch.h"
#include "pci.h"
#include <gatery/scl/utils/Thermometric.h>
#include <gatery/scl/utils/BitCount.h>

#include "../Fifo.h"
#include "../stream/StreamArbiter.h"

namespace gtry::scl::pci {
	TlpInstruction::operator sim::DefaultBitVectorState()
	{
		sim::DefaultBitVectorState packet;
		packet.resize(32);
		Helper helper(packet);
	
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
			case TlpOpcode::memoryReadRequest64bit: {
				packet.resize(128);
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
				HCL_DESIGNCHECK_HINT(this->requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(this->tag, "tag not set");
				HCL_DESIGNCHECK_HINT(this->address, "address not set");
				helper
					.write(this->requesterID >> 8, 8_b)
					.write(this->requesterID & 0xFF, 8_b)
					.write(this->tag, 8_b)
					.write(this->firstDWByteEnable, 4_b)
					.write(this->lastDWByteEnable, 4_b)
					.write((*this->address >> 56) & 0xFF, 8_b)
					.write((*this->address >> 48) & 0xFF, 8_b)
					.write((*this->address >> 40) & 0xFF, 8_b)
					.write((*this->address >> 32) & 0xFF, 8_b)
					.write((*this->address >> 24) & 0xFF, 8_b)
					.write((*this->address >> 16) & 0xFF, 8_b)
					.write((*this->address >> 8) & 0xFF, 8_b)
					.write(0, 2_b)
					.write((*this->address >> 2) & 0b00111111, 6_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::memoryWriteRequest64bit:
			{
				packet.resize(128);
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
				HCL_DESIGNCHECK_HINT(this->requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(this->tag, "tag not set");
				HCL_DESIGNCHECK_HINT(this->address, "address not set");
				HCL_DESIGNCHECK_HINT(this->payload, "you forgot to set the payload");
				helper
					.write(this->requesterID >> 8, 8_b)
					.write(this->requesterID & 0xFF, 8_b)
					.write(this->tag, 8_b)
					.write(this->firstDWByteEnable, 4_b)
					.write(this->lastDWByteEnable, 4_b)
					.write((*this->address >> 56) & 0xFF, 8_b)
					.write((*this->address >> 48) & 0xFF, 8_b)
					.write((*this->address >> 40) & 0xFF, 8_b)
					.write((*this->address >> 32) & 0xFF, 8_b)
					.write((*this->address >> 24) & 0xFF, 8_b)
					.write((*this->address >> 16) & 0xFF, 8_b)
					.write((*this->address >> 8) & 0xFF, 8_b)
					.write(0, 2_b)
					.write((*this->address >> 2) & 0b00111111, 6_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::completionWithData:
			{
				packet.resize(96);
				HCL_DESIGNCHECK_HINT(this->length, "length not set");
				HCL_DESIGNCHECK_HINT(this->completerID, "completer id not set");
				HCL_DESIGNCHECK_HINT(this->byteCount, "byteCount not set");
				HCL_DESIGNCHECK_HINT(this->requesterID, "requester id not set, this should come from your data requester");
				HCL_DESIGNCHECK_HINT(this->tag, "tag not set, it should come from your data requester");
				HCL_DESIGNCHECK_HINT(this->address, "address (lower address) not set, this corresponds to the byte address of the payload in the current TLP");
				HCL_DESIGNCHECK_HINT(this->payload, "you forgot to set the payload");

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
					.write(*this->address, 7_b)
					.skip(1_b);
				HCL_DESIGNCHECK_HINT(helper.offset == 96, "incomplete header");
				break;
			}
		}

		if (this->payload) {
			packet.resize(packet.size() + *this->length * 32);
			for (size_t i = 0; i < *this->length; i++)
				helper.write((*this->payload)[i], 32_b);
		}

		return packet;
	}

	uint64_t readState(sim::DefaultBitVectorState raw, size_t offset, size_t size) {
		HCL_DESIGNCHECK_HINT(sim::allDefined<sim::DefaultConfig>(raw, offset, size), "the extracted bit vector is not fully defined");
		return raw.extract(sim::DefaultConfig::VALUE, offset, size);
	}


	TlpInstruction TlpInstruction::createFrom(const sim::DefaultBitVectorState& raw){
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

		inst.opcode = (TlpOpcode) readState(raw, 0, 8);
		bool isRW;
		bool hasPayload;
		size_t hdrSize;

		switch(inst.opcode){
		case TlpOpcode::memoryReadRequest64bit:
			isRW = true;
			hasPayload = false;
			hdrSize = 128;
			break;
		case TlpOpcode::memoryWriteRequest64bit:
			isRW = true;
			hasPayload = true;
			hdrSize = 128;
		break;
		case TlpOpcode::completionWithData:
			isRW = false;
			hasPayload = true;
			hdrSize = 96;
			break;
		}
		
		if (isRW) {
			inst.requesterID = readState(raw, 32, 8) << 8;
			inst.requesterID |= readState(raw, 40, 8) & 0xFF;
			inst.tag = (uint8_t) readState(raw, 48, 8);
			inst.firstDWByteEnable = readState(raw, 56, 4);
			inst.lastDWByteEnable  = readState(raw, 60, 4);
			inst.address = readState(raw, 64, 8) << 56;
			*inst.address |= readState(raw, 72, 8) << 48;
			*inst.address |= readState(raw, 80, 8) << 40;
			*inst.address |= readState(raw, 88, 8) << 32;
			*inst.address |= readState(raw, 96, 8) << 24;
			*inst.address |= readState(raw, 104, 8) << 16;
			*inst.address |= readState(raw, 112, 8) << 8;
			*inst.address |= readState(raw, 122, 6) << 2;
			inst.ph = (uint8_t) readState(raw, 120, 2);
		}
		else {
			inst.completerID = readState(raw, 32, 8) << 8;
			*inst.completerID |= readState(raw, 40, 8) & 0xFF;
			inst.byteCount = readState(raw, 48, 4) << 8;
			inst.byteCountModifier = readState(raw, 52, 1);
			inst.completionStatus = readState(raw, 53, 3);
			*inst.byteCount |= readState(raw, 56, 8);
			inst.requesterID = readState(raw, 64, 8) << 8;
			inst.requesterID |= readState(raw, 72, 8) & 0xFF;
			inst.tag = (uint8_t) readState(raw, 80, 8);
			inst.address = readState(raw, 88, 7);
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


	std::ostream& operator << (std::ostream& s, const TlpInstruction& inst) {
		HCL_DESIGNCHECK_HINT(false, "not yet implemented");
	}

	//UInt min(UInt a, const UInt b) {
	//	UInt ret = b;
	//	IF(a < b) ret = a;
	//	return ret;
	//}
	//
	//
	//BVec computeByteMask(BitWidth dataW, UInt beatWordLength, BVec firstBe, BVec lastBe, Bit isSop, Bit isEop) {
	//	
	//	HCL_DESIGNCHECK(firstBe.width() == 4_b);
	//	HCL_DESIGNCHECK(lastBe.width() == 4_b);
	//	sim_assert(beatWordLength != 0) << "impossible beat word length, must be at least 1";
	//
	//	BVec mask = BitWidth(dataW.bytes());
	//	mask = 0;
	//	UInt beatByteLength = cat(beatWordLength, "2b0");
	//	mask = uintToThermometric(beatByteLength);
	//	auto maskParts = mask.parts(mask.width().value / 4);
	//	IF(isEop) maskParts[beatWordLength - 1] = lastBe;
	//	IF(isSop) maskParts[0] = firstBe;
	//}
	//
	Bit needsMultipleBeats(UInt address, UInt length, const BitWidth TLDataW) {
		UInt wordAddress = address.upper(-2_b);
		const size_t numWords = TLDataW.value / 32;
		UInt beatWordAddress = wordAddress.lower(BitWidth::count(numWords));
	
		return zext(beatWordAddress, +1_b) + length <= beatWordAddress.width().count();
	}



	pci::CompleterInterface makeTileLinkMaster(scl::TileLinkUL&& tl, BitWidth tlpW)
	{
		Area area{ "tlp_to_tileLink", true };

		TlpPacketStream<EmptyBits> complReq(tlpW);
		complReq.set(EmptyBits{ BitWidth::count(tlpW.bits()) });
									
		TlpPacketStream<EmptyBits> complCompl(tlpW);
		complCompl.set(EmptyBits{ BitWidth::count(tlpW.bits()) });

		setName(valid(complReq), "valid_checkpoint_3"); tap(valid(complReq));
		HCL_NAMED(tl);

		HCL_DESIGNCHECK_HINT(complReq->width() >= 128_b, "this design is limited to request widths that can accommodate an entire 4dw header into one beat");
		HCL_DESIGNCHECK_HINT(complCompl->width() >= 96_b, "this design is limited to completion widths that can accommodate an entire 3dw header into one beat");
		HCL_DESIGNCHECK_HINT(complReq->width() == complReq->width(), "artificial limitation for ease of implementation");
		HCL_DESIGNCHECK_HINT(tl.a->source.width() == pack(tlpAnswerInfo{}).width(), "the source width is not adequate");

		//-----------------TILELINK A GENERATION--------------------//
		Header hdr;
		unpack(complReq->lower(128_b), hdr); HCL_NAMED(hdr);

		//extract easy information
		tlpAnswerInfo txid;
		txid.dw0 = hdr.dw0;
		txid.requesterID = hdr.dw1.lower(16_b);
		txid.tag = hdr.dw1(16, 8_b);
		BVec firstBe = hdr.dw1(24, 4_b) ;
		txid.error |= firstBe != 0xF; // no byte addressability yet
		BVec lastBe	 = hdr.dw1(28, 4_b) ;
		txid.error |= lastBe  != 0x0; //payload = 1dw -> this needs to be zero;
		UInt lengthInWords = hdr.dataSize();
		txid.error |= lengthInWords != 1; //one word allowed only
		txid.error |= (emptyBits(complReq) != 512 - 4 * 32 & emptyBits(complReq) != 512 - 5 * 32);
		
		//deal with address calculations
		UInt tlpAddress = cat(swapEndian(hdr.dw2, 8_b), swapEndian(hdr.dw3, 8_b));
		tlpAddress.lower(2_b) = 0; //overwrite lower bits
		
		HCL_NAMED(tlpAddress);

		UInt address = tlpAddress.lower(tl.a->address.width());
		txid.lowerAddress = address.lower(txid.lowerAddress.width());
		

		tl.a->setupGet(address, pack(txid), 2);
		Bit isWrite = hdr.dw0.fmt.upper(2_b) == 0b01;
		IF(isWrite) {
			BVec data = (*complReq)(128, 32_b);
			tl.a->setupPut(address, data, pack(txid), 2);
		}
		
		Bit isRead  = hdr.dw0.fmt.upper(2_b) == 0b00;

		txid.error |= !isRead & !isWrite;
		
		valid(tl.a) = valid(complReq);
		ready(complReq) = ready(tl.a);

		HCL_NAMED(complReq);
		
		//-----------------TILELINK D PARSING--------------------//

		tlpAnswerInfo ans;
		unpack((*tl.d)->source, ans);
		ans.error |= (*tl.d)->error;
		
		// header populating
		Header completionHdr;
		completionHdr = allZeros(completionHdr);
		
		completionHdr.dw0 = ans.dw0;
		completionHdr.opcode(TlpOpcode::completionWithData);
		completionHdr.requesterId(ans.requesterID, false);
		completionHdr.tag(ans.tag, false);
		completionHdr.completionStatus(CompletionStatus::successfulCompletion);
		IF(ans.error) completionHdr.completionStatus(CompletionStatus::unsupportedRequest);
		
		completionHdr.byteCount(4);
		completionHdr.lowerAddress(ans.lowerAddress, true);
		
		//payload steering into position
		(*complCompl) = ConstBVec(complCompl->width());
		(*complCompl)(96, 32_b) = (*tl.d)->data;
		complCompl->lower(96_b) = (BVec) pack(completionHdr).lower(96_b);

		setName(complCompl->lower(128_b), "test");
		
		complCompl.set(EmptyBits{ 384 });

		setName(ans.error, "ERROR"); tap(ans.error);

		//handshake
		valid(complCompl) = valid((*tl.d)) & ((*tl.d)->hasData() | ans.error);
		eop(complCompl) = '1';
		emptyBits(complCompl) = 512 - 4 * 32;
		ready((*tl.d)) = ready(complCompl) | (valid((*tl.d)) & !(*tl.d)->hasData());
		
		HCL_NAMED(complReq);
		HCL_NAMED(complCompl);

		setName(valid(complCompl), "valid_checkpoint_4"); tap(valid(complCompl));

		return CompleterInterface{ move(complReq), move(complCompl) };
	}

}





namespace gtry::scl::pci::amd {

	TlpPacketStream<scl::EmptyBits> completerRequestVendorUnlocking(axi4PacketStream<CQUser>&& in)
	{
		Area area{ "completer_request_vendor_unlocking", true };
		setName(in, "axi_in");
		HCL_DESIGNCHECK_HINT(in->width() >= 128_b, "stream must be at least as big as 4dw for this implementation");
		/*
		* since we will work with 512 bit dw, we will definitely receive the entire descriptor within the first beat:
		* the parsing and translating should hence be quite simple
		*/
		struct Descriptor {
			BVec at = 2_b;
			BVec address = 62_b;
			BVec dwordCount = 11_b;
			BVec reqType = 4_b;
			Bit reservedDw2;
			BVec requesterID = 16_b;
			BVec tag = 8_b;
			BVec targetFunction = 8_b;
			BVec barId = 3_b;
			BVec barAperture = 6_b;
			BVec tc = 3_b;
			BVec attr = 3_b;
			Bit reservedDw3;
		};

		Descriptor desc;
		unpack(in->lower(128_b), desc);
		CQUser cqUser = in.get<CQUser>();

		Bit isWrite = desc.reqType == 0b0001;
		Bit isRead = desc.reqType == 0b0000;

		Header hdr;

		hdr.dw0.fmt = 0b001;
		IF(isWrite) hdr.dw0.fmt = 0b011;

		hdr.dw0.type = 0b0'0000;
		hdr.dw0.tc = desc.tc;
		hdr.dw0.idBasedOrderingAttr2 = desc.attr[2];
		hdr.dw0.th = cqUser.tph_present.at(0);

		hdr.dw0.td = '0'; //no tlp digest
		hdr.dw0.ep = '0'; //not poinsoned
		hdr.dw0.relaxedOrderingAttr1 = desc.attr[1];
		hdr.dw0.noSnoopAttr0 = desc.attr[0];
		hdr.dw0.at = desc.at;
		hdr.dw0.length(desc.dwordCount);

		hdr.dw1 = (BVec)cat(cqUser.last_be.lower(4_b), cqUser.first_be.lower(4_b), desc.tag, desc.requesterID.lower(8_b), desc.requesterID.upper(8_b));
		hdr.dw2 = swapEndian(desc.address.upper(32_b), 8_b);
		hdr.dw3 = (BVec) swapEndian(cat(desc.address.lower(30_b), "2b0"), 8_b);
		hdr.dw3.lower(2_b) = 0; //procesing hint thingy

		TlpPacketStream<scl::EmptyBits> ret(in->width());

		//data assignments
		*ret = *in;
		IF(sop(in))
			ret->lower(128_b) = (BVec) pack(hdr);

		//Handshake assignments
		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//keep to empty conversion:
		UInt emptyWords = thermometricToUInt(~keep(in)).lower(-1_b); HCL_NAMED(emptyWords);
		emptyBits(ret) = cat(emptyWords, "5b0");
		setName(ret, "tlp_out");
		setName(valid(ret), "valid_checkpoint_2"); tap(valid(ret));
		return ret;
	}

	axi4PacketStream<CCUser> completerCompletionVendorUnlocking(TlpPacketStream<EmptyBits>&& in)
	{
		Area area{ "completer_completion_vendor_unlocking", true };
		setName(in, "tlp_in");
		HCL_DESIGNCHECK_HINT(in->width() >= 96_b, "stream must be at least as big as 3dw for this implementation");

		BVec hdrBvec = in->lower(96_b);

		Header hdr;
		unpack(zext(hdrBvec, 128_b), hdr);

		struct Descriptor {
			BVec address = 7_b;
			Bit reservedDw0_0;
			BVec at = 2_b;
			BVec reservedDw0_1 = 6_b;
			BVec byteCount = 13_b;
			Bit lockedReadCompletion;
			BVec reservedDw0_3 = 2_b;
			UInt dwordCount = 11_b;
			BVec completionStatus = 3_b;
			Bit poisonedCompletion;
			Bit reservedDw1;
			BVec requesterID = 16_b;
			BVec tag = 8_b;
			BVec completerID = 16_b;
			Bit completerIdEnable;
			BVec tc = 3_b;
			BVec attr = 3_b;
			Bit forceECRC;
		};

		Descriptor desc;
		desc.address = hdr.dw2(24, 7_b);
		desc.at = hdr.dw0.at;
		desc.byteCount = (BVec)cat('0', hdr.dw1(16, 4_b), hdr.dw1(24, 8_b));
		desc.lockedReadCompletion = hdr.dw0.type == 0b01011;
		desc.dwordCount = cat('0', hdr.dw0.length());
		desc.completionStatus = hdr.dw1(21, 3_b);
		desc.poisonedCompletion = hdr.dw0.ep;
		desc.requesterID = swapEndian(hdr.dw2.lower(16_b), 8_b);
		desc.tag = hdr.dw2(16, 8_b);
		desc.completerID = swapEndian(hdr.dw1.lower(16_b), 8_b);
		desc.completerIdEnable = '0';
		desc.tc = hdr.dw0.tc;
		desc.attr = (BVec) cat(hdr.dw0.idBasedOrderingAttr2, hdr.dw0.relaxedOrderingAttr1, hdr.dw0.noSnoopAttr0);
		desc.forceECRC = '0';

		CCUser ccUser = allZeros(CCUser{});
		HCL_NAMED(ccUser);

		axi4PacketStream<CCUser> ret(in->width());
		ret.get<CCUser>() = ccUser;

		//data assignments
		*ret = *in;
		IF(sop(in))
			ret->lower(96_b) = (BVec) pack(desc);

		//Handshake assignments
		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//empty to full words conversion:
		sim_assert(emptyBits(in).lower(5_b) == 0); // the granularity of this empty signal cannot be less than 32 bits (expect the lsb's to be completely synthesized away)
		UInt emptyWords = emptyBits(in).upper(-5_b);

		BVec throwAway = (BVec) cat('0',uintToThermometric(emptyWords));

		keep(ret) = swapEndian(~throwAway, 1_b); // free op
		setName(ret, "axi_out");
		setName(valid(ret), "valid_checkpoint_5"); tap(valid(ret));
		return ret;
	}
}
