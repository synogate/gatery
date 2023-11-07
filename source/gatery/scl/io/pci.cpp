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
#include <gatery/scl/utils/BitCount.h>

#include "../Fifo.h"
#include "../stream/StreamArbiter.h"

namespace gtry::scl::pci {

	void SimTlp::makeHeader(TlpInstruction instr)
	{
		header.resize(128); //make it always be 4 dw for now
		Helper helper(header);

		helper
			.write(instr.opcode, 8_b)
			.write(instr.th, 1_b)
			.skip(1_b)
			.write(instr.idBasedOrderingAttr2, 1_b)
			.skip(1_b)
			.write(instr.tc, 3_b)
			.skip(1_b)
			.write(*instr.length >> 8, 2_b)
			.write(instr.at, 2_b)
			.write(instr.noSnoopAttr0, 1_b)
			.write(instr.relaxedOrderingAttr1, 1_b)
			.write(instr.ep, 1_b)
			.write(instr.td, 1_b)
			.write(*instr.length & 0xFF, 8_b);

		HCL_DESIGNCHECK(helper.offset == 32);

		switch (instr.opcode) {
			case TlpOpcode::memoryReadRequest64bit: {
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set");
				HCL_DESIGNCHECK_HINT(instr.address, "address not set");
				helper
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.write((*instr.address >> 56) & 0xFF, 8_b)
					.write((*instr.address >> 48) & 0xFF, 8_b)
					.write((*instr.address >> 40) & 0xFF, 8_b)
					.write((*instr.address >> 32) & 0xFF, 8_b)
					.write((*instr.address >> 24) & 0xFF, 8_b)
					.write((*instr.address >> 16) & 0xFF, 8_b)
					.write((*instr.address >> 8) & 0xFF, 8_b)
					.write((*instr.address) & 0x11111100, 6_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::memoryWriteRequest64bit:
			{
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set");
				HCL_DESIGNCHECK_HINT(instr.address, "address not set");
				helper
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.write((*instr.address >> 56) & 0xFF, 8_b)
					.write((*instr.address >> 48) & 0xFF, 8_b)
					.write((*instr.address >> 40) & 0xFF, 8_b)
					.write((*instr.address >> 32) & 0xFF, 8_b)
					.write((*instr.address >> 24) & 0xFF, 8_b)
					.write((*instr.address >> 16) & 0xFF, 8_b)
					.write((*instr.address >> 8) & 0xFF, 8_b)
					.write((*instr.address) & 0x11111100, 6_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::completionWithData:
			{
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.completerID, "completer id not set");
				HCL_DESIGNCHECK_HINT(instr.byteCount, "byteCount not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set, this should come from your data requester");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set, it should come from your data requester");
				helper
					//START HERE
					.write(*instr.completerID >> 8, 8_b)
					.write(*instr.completerID & 0xFF, 8_b)
					.write(*instr.byteCount >> 8, 4_b)
					.write(instr.byteCountModifier, 1_b)
					.write(instr.completionStatus, 3_b)
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 96);
				break;
			}
		}
	}

	UInt min(UInt a, const UInt b) {
		UInt ret = b;
		IF(a < b) ret = a;
		return ret;
	}

	BVec uintToThermometer(UInt in) {
		BVec ret = BitWidth(in.width().last());
		for (size_t i = 0; i < ret.size(); i++)
			ret.at(i) = in > i;
		return ret;
	}

	BVec computeByteMask(BitWidth dataW, UInt beatWordLength, BVec firstBe, BVec lastBe, Bit isSop, Bit isEop) {
		
		HCL_DESIGNCHECK(firstBe.width() == 4_b);
		HCL_DESIGNCHECK(lastBe.width() == 4_b);

		BVec mask = BitWidth(dataW.bytes());
		mask = 0;
		UInt beatByteLength = cat(beatWordLength, "2b0");
		mask = uintToThermometer(beatByteLength);
		IF(isEop) mask. = lastBe;
		IF(isSop) mask.word(0, 4_b) = firstBe;
		//needs rewrite with parts 
	}



	TileLinkUL makeTileLinkMaster(TlpPacketStream<EmptyBits>&& complReq, const TlpPacketStream<EmptyBits>& complCompl)
	{
		HCL_DESIGNCHECK_HINT(complReq->width() >= 128_b, "this design is limited to request widths that can accomodate an entire 4dw header into one beat");
		HCL_DESIGNCHECK_HINT(complCompl->width() >= 96_b, "this design is limited to completion widths that can accomodate an entire 3dw header into one beat");
		HCL_DESIGNCHECK_HINT(complReq->width() == complReq->width(), "artificial limitation for ease of implementation");

		TileLinkUL tl = tileLinkInit<TileLinkUL>(64_b, complReq->width(), 24_b);
		tl = dontCare(tl);

		//decode tlp into tilelinkA
		Header hdr;

		//determine if it's a write
		UInt address = cat(hdr.dw2.dw, hdr.dw3.dw);
		address.lower(2_b) = 0; //overwrite lower bits
		BVec txid = hdr.dw1.dw.lower(24_b);

		Bit isWrite = hdr.dw0.fmt.upper(2_b) == 0b01;
		IF(isWrite) {
			IF(sop(in)) {
				BVec data = *complReq >> 128;
				UInt lengthInWords = hdr.dataSize();
				const size_t beatWords = complReq->width().value / 32 - 4;
				UInt beatLengthInWords = min(lengthInWords, beatWords);

				BVec mask = BitWidth(complReq->width().bytes());

				mask.word(0, 4_b) = hdr.dw1.dw.word(24, 4_b);

				tl.a->setupPutPartial(address,data,)
			}


			IF(partial)
				tl.a->setupPutPartial(;
			IF(!partial)
				tl.a->setupPut();
		}

		Bit isRead  = hdr.dw0.fmt.upper(2_b) == 0b00;
		IF(isRead) {
			UInt lengthInWords = hdr.dataSize();
			UInt lengthInBytes = cat(lengthInWords, "2b0");
			UInt size = utils::Log2C(lengthInBytes);
			tl.a->setupGet(address, (UInt) txid, size);
		}

		unpack(complReq->lower(128_b), hdr);
		ENIF(valid(in) & sop(in)) hdr = reg(hdr);







		
		UInt payloadsizeInWordsLeft = hdr.dataSize(); // initial condition

		const size_t fullBeatSizeInWords = (complReq->width().value / 32);
		const size_t headerSizeInWords = 4;
		const size_t firstBeatSizeInWords = fullBeatSizeInWords - headerSizeInWords;

		UInt sizeInWords = constructFrom(fullBeatSizeInWords);

		sizeInWords = fullBeatSizeInWords;
		IF(sop(in)) sizeInWords = fullBeatSizeInWords - headerSizeInWords;
		IF(eop(in)) sizeInWords -= payloadsizeInWordsLeft;

		UInt sizeInBytes = cat(sizeInWords, "2b0");

		

		ENIF(sop(in)) {
			*tl.a = reg(*tl.a);
			PayloadsizeInWordsLeft  = reg(PayloadsizeInWordsLeft)
		}
		











		return TileLinkUL();
	}
}





namespace gtry::scl::pci::amd {

	TlpPacketStream<scl::EmptyBits> completerRequestVendorUnlocking(axi4PacketStream<CQUser> in)
	{
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
		hdr.dw0.idBasedOrderingAttr2 = desc.attr.at(2);
		hdr.dw0.th = cqUser.tph_present.at(0);
		hdr.dw0.td = '0'; //no tlp digest
		hdr.dw0.ep = '0'; //not poinsoned
		hdr.dw0.relaxedOrderingAttr1 = desc.attr.at(1);
		hdr.dw0.noSnoopAttr0 = desc.attr.at(0);
		hdr.dw0.at = desc.at;
		hdr.dw0.length(desc.dwordCount);

		hdr.dw1.dw = (BVec)cat(cqUser.last_be, cqUser.first_be, desc.tag, desc.requesterID);
		hdr.dw2.dw = desc.address.upper(32_b);
		hdr.dw3.dw.upper(32_b) = desc.address.lower(32_b);
		hdr.dw3.dw.lower(2_b) = 0; //procesing hint thingy


		TlpPacketStream<scl::EmptyBits> ret(in->width());

		//data assignments
		*ret = *in;
		IF(valid(in) & sop(in))
			ret->lower(128_b) = (BVec) pack(hdr);

		//Handshake assignments
		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//keep to empty conversion:
		UInt emptyWords = bitcount((UInt) ~keep(in));
		emptyBits(ret) = cat(emptyWords, "5b0");

		return ret;
	}

	axi4PacketStream<CCUser> completerCompletionVendorUnlocking(TlpPacketStream<EmptyBits> in) {

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
			BVec dwordCount = 11_b;
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
		desc.address = hdr.dw2.dw.word(24, 7_b);
		desc.at = hdr.dw0.at;
		desc.byteCount = (BVec) cat(hdr.dw1.dw.word(16, 4_b), hdr.dw1.dw.word(24, 8_b));
		desc.lockedReadCompletion = hdr.dw0.type == 0b01011;
		desc.dwordCount = hdr.dw0.length();
		desc.completionStatus = hdr.dw1.dw.word(21, 3_b);
		desc.poisonedCompletion = hdr.dw0.ep;
		desc.requesterID = hdr.dw2.dw.lower(16_b);
		desc.tag = hdr.dw1.dw.word(16, 8_b);
		desc.completerID = hdr.dw1.dw.lower(16_b);
		desc.completerIdEnable = '1';
		desc.tc = hdr.dw0.tc;
		desc.attr = (BVec) cat(hdr.dw0.idBasedOrderingAttr2, hdr.dw0.relaxedOrderingAttr1, hdr.dw0.noSnoopAttr0);
		desc.forceECRC = '0';

		CCUser ccUser = { BVec(0) }; // check this working

		axi4PacketStream<CCUser> ret(in->width());
		ret.set(ccUser);

		//data assignments
		*ret = *in;
		IF(valid(in) & sop(in))
			ret->lower(96_b) = (BVec) pack(desc);

		//Handshake assignments
		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//empty to full words conversion:
		sim_assert(emptyBits(in).lower(5_b) == 0); // the granularity of this empty signal cannot be less than 32 bits (expect the lsb's to be completely synthesized away)
		UInt emptyWords = emptyBits(in).upper(-5_b);
		size_t numWords = in->width().value / 32;
		UInt fullWords = numWords - emptyWords;

		//full words to keep conversion:
		BVec keepBVec = BitWidth(numWords);
		for (size_t i = 0; i < numWords; i++)
			keepBVec.at(i) = fullWords > i;

		keep(ret) = keepBVec;
		return ret;
	}
}
