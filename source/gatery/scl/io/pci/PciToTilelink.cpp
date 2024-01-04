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
#include "PciToTileLink.h"
#include <gatery/scl/utils/math.h>
#include <gatery/scl/utils/Thermometric.h>
#include <gatery/scl/utils/bitCount.h>
#include <gatery/scl/utils/OneHot.h>

#include <gatery/scl/Fifo.h>

namespace gtry::scl::pci {

	void TlpAnswerInfo::setErrorFromLimitations(RequestHeader reqHdr) 
	{
		error |= reqHdr.firstDWByteEnable != 0xF;	// no byte addressability yet
		error |= reqHdr.lastDWByteEnable != 0x0;	//payload = 1dw -> this needs to be zero;
		error |= reqHdr.common.length != 1;			//one word allowed only
	}

	TlpAnswerInfo TlpAnswerInfo::fromRequest(RequestHeader reqHdr)
	{
		TlpAnswerInfo ret;
		ret.common = reqHdr.common;
		ret.requesterID = reqHdr.requesterId;
		ret.tag = reqHdr.tag;
		ret.lowerByteAddress = cat(reqHdr.wordAddress, "2b00").lower(7_b);
		ret.setErrorFromLimitations(reqHdr);
		return ret;
	}
	TlpPacketStream<EmptyBits, BarInfo> completerRequestToTileLinkA(TileLinkChannelA& a, BitWidth tlpStreamW)
	{
		Area area{ "CRToTileLinkA", true };
		TlpPacketStream<EmptyBits, BarInfo> complReq(tlpStreamW);
		HCL_DESIGNCHECK_HINT(complReq->width() >= 128_b, "this design is limited to completion widths that can accommodate an entire 3dw header into one beat");
		complReq.set(EmptyBits{ BitWidth::count(tlpStreamW.bits()) });

		RequestHeader reqHdr = RequestHeader::fromRaw(complReq->lower(128_b));
		HCL_NAMED(reqHdr);
		TlpAnswerInfo answerInfo = TlpAnswerInfo::fromRequest(reqHdr);
		HCL_DESIGNCHECK(width(answerInfo) == width(TlpAnswerInfo{}));

		//make sure that the bar aperture is large enough to accommodate the TileLink interface

		answerInfo.error |= complReq.get<BarInfo>().logByteAperture < a->address.width().bits();
		IF(valid(complReq))
			sim_assert(complReq.get<BarInfo>().logByteAperture >= a->address.width().bits()) << "the bar aperture is not adequate";
		UInt byteAddress = cat(reqHdr.wordAddress, "2b00").lower(a->address.width());

		a->setupGet(byteAddress, pack(answerInfo), 2);

		IF(reqHdr.common.isMemWrite()) {
			BVec data = (*complReq)(128, 32_b);
			a->setupPut(byteAddress, data, pack(answerInfo), 2);
		}

		answerInfo.error |= !reqHdr.common.isMemRead() & !reqHdr.common.isMemWrite();

		valid(a) = valid(complReq);
		ready(complReq) = ready(a);

		HCL_NAMED(complReq);
		return complReq;
	}


	TlpPacketStream<EmptyBits> tileLinkDToCompleterCompletion(TileLinkChannelD&& d, BitWidth tlpStreamW)
	{
		Area area{ "tileLinkDToCC", true };
		TlpAnswerInfo ans;
		unpack(d->source, ans);
		ans.error |= d->error; 
		ans.common.opcode(TlpOpcode::completionWithData);

		BVec compStatus = ConstBVec((size_t)CompletionStatus::successfulCompletion,  3_b);
		IF(ans.error) compStatus = (size_t)CompletionStatus::unsupportedRequest;

		CompletionHeader completionHdr{
			.common = ans.common,
			.requesterId = ans.requesterID,
			.tag = ans.tag,
			.completerId = ConstBVec(0, 16_b),
			.byteCount = ConstUInt(4, 12_b),
			.byteCountModifier = '0',
			.lowerByteAddress = ans.lowerByteAddress,
			.completionStatus = compStatus,
		};
		HCL_NAMED(completionHdr);

		TlpPacketStream<EmptyBits> complCompl(tlpStreamW);
		HCL_DESIGNCHECK_HINT(complCompl->width() >= 96_b, "this design is limited to completion widths that can accommodate an entire 3dw header into one beat");
		complCompl.set(EmptyBits{ BitWidth::count(tlpStreamW.bits()) });
		(*complCompl) = ConstBVec(complCompl->width());
		(*complCompl)(96, 32_b) = d->data;
		complCompl->lower(96_b) = (BVec) completionHdr;

		setName(ans.error, "ERROR");

		valid(complCompl) = valid(d) & (d->hasData() | ans.error);
		eop(complCompl) = '1';
		emptyBits(complCompl) = tlpStreamW.bits() - 4 * 32;
		ready(d) = ready(complCompl) | (valid(d) & !d->hasData());

		return complCompl;
	}

	CompleterInterface makeTileLinkMaster(scl::TileLinkUL&& tl, BitWidth tlpW)
	{
		Area area{ "makeTileLinkMaster", true };
		HCL_NAMED(tl);

		HCL_DESIGNCHECK_HINT(tl.a->source.width() == width(TlpAnswerInfo{}), "the source width is not adequate");
	
		TlpPacketStream complReq = completerRequestToTileLinkA(tl.a, tlpW);
		HCL_NAMED(complReq);
		
		TlpPacketStream<EmptyBits> complCompl = tileLinkDToCompleterCompletion(move(*tl.d), tlpW);
		HCL_NAMED(complCompl);
		
		return CompleterInterface{ move(complReq), move(complCompl) };
	}

	static UInt logBytesToBytes(const UInt& logBytes) {
		UInt bytes = ConstUInt(1, BitWidth(logBytes.width().count())) << logBytes;
		return bytes;
	}

	static UInt length(const UInt& bytes, const UInt& byteAddress) {
		UInt ret = "11d0";
		ret = 1;
		IF(zext(bytes) > 4) {
			ret = zext(bytes >> 2);
			IF(byteAddress.lower(2_b) != 0) {
				ret += 1;
			}
		}
		return ret;
	}

	static BVec firstDwByteEnable(const UInt& byteAddress) {
		return (BVec) cat('1', ~uintToThermometric(byteAddress.lower(2_b)));
	}

	static BVec lastDwByteEnable(const UInt& bytes, const UInt& byteAddress) {
		BVec ret = 4_b;
		ret = "4b1111";
		IF(byteAddress.lower(2_b) != 0) {
			ret = (BVec) cat('0', uintToThermometric(byteAddress.lower(2_b)));
		}
		IF(zext(bytes) <= 4) {
			ret = 0;
		}
		return ret;
	}

	TlpPacketStream<EmptyBits> tileLinkAToRequesterRequest(TileLinkChannelA&& a, BitWidth tlpW)
	{
		Area area{ "TL_A_to_requester_request_tlp", true };

		TlpPacketStream<EmptyBits> rr(tlpW);
		emptyBits(rr) = BitWidth::count(rr->width().bits());

		RequestHeader hdr;
		hdr.common.poisoned= '0';
		hdr.common.digest= '0';
		hdr.common.processingHintPresence = '0';
		hdr.common.attributes = {'0','0','0'};
		hdr.common.addressType = (size_t) AddressType::defaultOption;
		hdr.common.trafficClass = (size_t) TrafficClass::defaultOption;
		hdr.common.opcode(TlpOpcode::memoryReadRequest64bit);

		IF(valid(a) & a->isPut()) {
			sim_assert('0') << "untested, consider it non-working";
			hdr.common.opcode(TlpOpcode::memoryWriteRequest64bit);
		}

		UInt bytes = logBytesToBytes(a->size);
		setName(bytes, "rr_bytes");

		hdr.common.dataLength(length(bytes, a->address));

		hdr.requesterId = 0;
		HCL_DESIGNCHECK_HINT(a->source.width() <= 8_b, "source is too large for the fixed (non-extended) tag field");
		hdr.tag = (BVec) zext(a->source);

		hdr.lastDWByteEnable = lastDwByteEnable(bytes, a->address);
		hdr.firstDWByteEnable = firstDwByteEnable(a->address);
		hdr.wordAddress = zext(a->address.upper(-2_b));
		hdr.processingHint = (size_t) ProcessingHint::defaultOption;
		setName(hdr, "rr_hdr");

		*rr = 0;
		IF(sop(a)) {
			rr->lower(128_b) = (BVec)hdr;
		}

		valid(rr) = valid(a);
		eop(rr) = eop(a);
		ready(a) = ready(rr);
		emptyBits(rr) = rr->width().bits() - 4 * 32;

		return rr;
	}

	static UInt logByteSize(const UInt& lengthInBytes) {
		
		return encoder((OneHot) lengthInBytes);
	}

	TileLinkChannelD requesterCompletionToTileLinkD(TlpPacketStream<EmptyBits>&& rc, BitWidth byteAddressW, BitWidth dataW)
	{
		Area area{ "requester_completion_tlp_to_TL_D", true };
		const CompletionHeader hdr = CompletionHeader::fromRaw(rc->lower(96_b));

		TileLinkChannelD d = move(*(tileLinkInit<TileLinkUL>(byteAddressW, dataW, 8_b).d));
		//HCL_DESIGNCHECK_HINT(d->data.width() == 32_b, "not yet implemented");

		d->opcode = (size_t) TileLinkD::OpCode::AccessAckData;
		d->source = (UInt) hdr.tag;
		d->sink = 0;
		d->param = 0;
		d->error = hdr.common.poisoned | (hdr.completionStatus != (size_t) CompletionStatus::successfulCompletion);

		IF(valid(rc) & sop(rc))
			sim_assert(bitcount(hdr.byteCount) == 1) << "TileLink cannot represent non powers of 2 amount of bytes";

		UInt size = logByteSize(hdr.byteCount);
		IF(valid(rc) & sop(rc))
			sim_assert(size < d->size.width().count()) << "breaking this assertion invalidates the next line's truncation";
		d->size = size.lower(d->size.width());

		BVec headerlessData = *rc >> 96;
		d->data = (headerlessData << cat(hdr.lowerByteAddress(2, 3_b),"5b00000")).lower(d->data.width());

		valid(d) = valid(rc);
		ready(rc) = ready(d);
		//IF(valid(rc) & sop(rc))
		//	sim_assert(emptyBits(rc) == (rc->width().bits() - (3*32 + ) )) << "only support one word right now";

		return d;
	}


	

	TileLinkChannelD requesterCompletionToTileLinkDMultipleBeats(TlpPacketStream<EmptyBits>&& rc, BitWidth byteAddressW)
	{
		Area area{ "requester_completion_tlp_to_TL_D", true };
		CompletionHeader hdr = CompletionHeader::fromRaw(rc->lower(96_b));





		HCL_DESIGNCHECK_HINT(rc->width() >= 96_b, "the first beat must contain the entire header in this implementation");

		TileLinkChannelD d = move(*(tileLinkInit<TileLinkUL>(byteAddressW, rc->width(), 8_b).d));

		d->opcode = (size_t) TileLinkD::OpCode::AccessAckData;
		d->source = (UInt) hdr.tag;
		d->sink = 0;
		d->param = 0;
		d->error = hdr.common.poisoned | (hdr.completionStatus != (size_t) CompletionStatus::successfulCompletion);

		IF(valid(rc) & sop(rc))
			sim_assert(bitcount(hdr.byteCount) == 1) << "TileLink cannot represent non powers of 2 amount of bytes";

		UInt size = logByteSize(hdr.byteCount);
		IF(valid(rc) & sop(rc))
			sim_assert(size < d->size.width().count()) << "breaking this assertion invalidates the next line's truncation";
		d->size = size.lower(d->size.width());

		BVec headerlessData = *rc >> 96;
		d->data = (headerlessData << cat(hdr.lowerByteAddress(2, 3_b),"5b00000")).lower(d->data.width());

		valid(d) = valid(rc);
		ready(rc) = ready(d);
		//IF(valid(rc) & sop(rc))
		//	sim_assert(emptyBits(rc) == (rc->width().bits() - (3*32 + ) )) << "only support one word right now";

		return d;
	}

	TileLinkUL makePciMaster(RequesterInterface&& reqInt, BitWidth byteAddressW, BitWidth dataW, BitWidth tagW)
	{
		HCL_DESIGNCHECK_HINT(tagW <= 8_b, "pcie cannot accommodate more than 8 bit tags");
		TileLinkUL ret = tileLinkInit<TileLinkUL>(byteAddressW, dataW, tagW);

		TileLinkChannelA a = constructFrom(ret.a);
		a <<= ret.a;
		
		reqInt.request <<= tileLinkAToRequesterRequest(move(a), reqInt.request->width());
		*ret.d = requesterCompletionToTileLinkD(move(reqInt.completion), byteAddressW, dataW);
	
		return ret;
	}
}
