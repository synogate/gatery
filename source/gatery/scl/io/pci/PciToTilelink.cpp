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
#include "PciToTileLink.h"
#include <gatery/scl/utils/Thermometric.h>
#include <gatery/scl/utils/BitCount.h>
#include <gatery/scl/utils/OneHot.h>

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
		Area area{ "scl_CRToTileLinkA", true };
		TlpPacketStream<EmptyBits, BarInfo> complReq(tlpStreamW);
		HCL_DESIGNCHECK_HINT(complReq->width() >= 128_b, "this design is limited to completion widths that can accommodate an entire 3dw header into one beat");
		complReq.set(EmptyBits{ BitWidth::count(tlpStreamW.bits()) });

		RequestHeader reqHdr = RequestHeader::fromRaw(complReq->lower(128_b));
		HCL_NAMED(reqHdr);
		TlpAnswerInfo answerInfo = TlpAnswerInfo::fromRequest(reqHdr);
		HCL_DESIGNCHECK(width(answerInfo) == width(TlpAnswerInfo{}));

		//make sure that the bar aperture is large enough to accommodate the TileLink interface

		answerInfo.error |= complReq.get<BarInfo>().logByteAperture < a->address.width().bits();
		//IF(valid(complReq))
		//	sim_assert(complReq.get<BarInfo>().logByteAperture >= a->address.width().bits()) << "the bar aperture is not adequate";
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
		Area area{ "scl_tileLinkDToCC", true };
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
		Area area{ "scl_makeTileLinkMaster", true };
		HCL_NAMED(tl);

		HCL_DESIGNCHECK_HINT(tl.a->source.width() == width(TlpAnswerInfo{}), "the source width is not adequate");

		// changed TlPacketStream to auto because clang doens't have alias template argument deduction
		auto complReq = completerRequestToTileLinkA(tl.a, tlpW);
		HCL_NAMED(complReq);
		
		TlpPacketStream<EmptyBits> complCompl = tileLinkDToCompleterCompletion(move(*tl.d), tlpW);
		HCL_NAMED(complCompl);
		
		return CompleterInterface{ move(complReq), move(complCompl) };
	}

	static UInt length(const UInt& bytes, const UInt& byteAddress) {
		UInt ret = "11d1";

		ret = zext(bytes.upper(-2_b));
		IF(byteAddress.lower(2_b) != 0) {
			ret += 1;
		}

		return ret;
	}

	static BVec firstDwByteEnable(const UInt& byteAddress) {
		return (BVec) cat('1', ~uintToThermometric(byteAddress.lower(2_b)));
	}

	static BVec lastDwByteEnable(const UInt& bytes, const UInt& byteAddress) {

		UInt endByteAddress = byteAddress + zext(bytes);
		BVec ret = (BVec) zext(uintToThermometric(endByteAddress.lower(2_b)), 4_b);

		//we do not want this case to be 0000, but 1111 (full byte enable mask)
		IF(endByteAddress.lower(2_b) == 0)
			ret = (BVec) "4b1111";
		
		//by pci spec, if the packet is only 1 dw, last byte enable must be 0
		IF(zext(bytes) <= 4)
			ret = 0;

		return ret;
	}

	static RequestHeader fromTileLinkA(TileLinkA a) {
		RequestHeader hdr;
		hdr.common.poisoned= '0';
		hdr.common.digest= '0';
		hdr.common.processingHintPresence = '0';
		hdr.common.attributes = {'0','0','0'};
		hdr.common.addressType = (size_t) AddressType::defaultOption;
		hdr.common.trafficClass = (size_t) TrafficClass::defaultOption;
		hdr.common.opcode(TlpOpcode::memoryReadRequest64bit);

		IF(a.isPut()) {
			hdr.common.opcode(TlpOpcode::memoryWriteRequest64bit);
		}

		UInt bytes = (UInt) decoder(a.size);
		setName(bytes, "rr_bytes");

		hdr.common.dataLength(length(bytes, a.address));

		hdr.requesterId = 0;
		HCL_DESIGNCHECK_HINT(a.source.width() <= 8_b, "source is too large for the fixed (non-extended) tag field");
		hdr.tag = (BVec) zext(a.source);

		hdr.lastDWByteEnable = lastDwByteEnable(bytes, a.address);
		hdr.firstDWByteEnable = firstDwByteEnable(a.address);
		hdr.wordAddress = zext(a.address.upper(-2_b));
		hdr.processingHint = (size_t) ProcessingHint::defaultOption;
		
		return hdr;
	}

	TlpPacketStream<EmptyBits> tileLinkAToRequesterRequest(TileLinkChannelA&& a, std::optional<BitWidth> tlpW)
	{
		Area area{ "scl_TL_A_to_requester_request_tlp", true };
		BitWidth rrW = a->data.width();

		if (tlpW)
			rrW = *tlpW;

		TlpPacketStream<EmptyBits> rr(rrW);
		emptyBits(rr) = BitWidth::count(rr->width().bits());

		RequestHeader hdr = fromTileLinkA(*a);

		IF(valid(a)) {
			sim_assert(a->isGet()) << "non-get is untested";
			IF(zext(a->size) > zext(a->data.width().bits()))
				sim_assert(a->source.width() == 0_b) << "no support for multiple ongoing bursts yet";
		}

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

	TileLinkChannelD requesterCompletionToTileLinkD(TlpPacketStream<EmptyBits>&& rc, BitWidth byteAddressW, BitWidth dataW)
	{
		Area area{ "scl_requester_completion_tlp_to_TL_D", true };
		const CompletionHeader hdr = CompletionHeader::fromRaw(rc->lower(96_b));

		TileLinkChannelD d = constructFrom(*(tileLinkInit<TileLinkUL>(byteAddressW, dataW, 8_b).d));

		d->opcode = (size_t) TileLinkD::OpCode::AccessAckData;
		d->source = (UInt) hdr.tag;
		d->sink = 0;
		d->param = 0;
		d->error = hdr.common.poisoned | (hdr.completionStatus != (size_t) CompletionStatus::successfulCompletion);


		IF(valid(rc) & sop(rc))
			sim_assert(bitcount(hdr.byteCount) == 1) << "TileLink cannot represent non powers of 2 amount of bytes";
		UInt logByteSize = encoder((OneHot) hdr.byteCount);
		IF(valid(rc) & sop(rc))
			sim_assert(logByteSize < d->size.width().count()) << "breaking this assertion invalidates the next line's truncation";
		d->size = logByteSize.lower(d->size.width());

		BVec headerlessData = *rc >> 96;
		d->data = (headerlessData << cat(hdr.lowerByteAddress(2, 3_b),"5b00000")).lower(d->data.width());

		valid(d) = valid(rc);
		ready(rc) = ready(d);

		return d;
	}

	TileLinkChannelD requesterCompletionToTileLinkDFullW(TlpPacketStream<EmptyBits>&& rc)
	{
		Area area{ "requester_completion_tlp_to_TL_D", true };
		HCL_DESIGNCHECK_HINT(rc->width() >= 96_b, "the first beat must contain the entire header in this implementation");

		CompletionHeader hdr = CompletionHeader::fromRaw(rc->lower(96_b));

		ENIF(valid(rc) & sop(rc)) {
			hdr = reg(hdr); // captures and holds the header before it gets squashed by the shift right.
		}

		auto rcPayloadStream = scl::strm::streamShiftRight(move(rc), 96);


		TileLinkChannelD d = constructFrom(*(tileLinkInit<TileLinkUL>(64_b, rcPayloadStream->width(), 8_b).d));

		d->data = *rcPayloadStream;
		d->opcode = (size_t) TileLinkD::OpCode::AccessAckData;
		d->source = (UInt) hdr.tag;
		d->sink = 0;
		d->param = 0;
		d->error = hdr.common.poisoned | (hdr.completionStatus != (size_t) CompletionStatus::successfulCompletion);


		IF(valid(rcPayloadStream) & sop(rcPayloadStream))
			sim_assert(bitcount(hdr.byteCount) == 1) << "TileLink cannot represent non powers of 2 amount of bytes";
		UInt logByteSize = encoder((OneHot) hdr.byteCount);
		IF(valid(rcPayloadStream) & sop(rcPayloadStream))
			sim_assert(logByteSize < d->size.width().count()) << "breaking this assertion invalidates the next line's truncation";
		d->size = logByteSize.lower(d->size.width());

		valid(d) = valid(rcPayloadStream);
		ready(rcPayloadStream) = ready(d);

		return d;
	}
	

	TileLinkChannelD requesterCompletionToTileLinkDCheapBurst(TlpPacketStream<EmptyBits>&& rc, std::optional<BitWidth> sizeW)
	{
		Area area{ "requester_completion_tlp_to_TL_D", true };
		HCL_DESIGNCHECK_HINT(rc->width() >= 96_b, "the first beat must contain the entire header in this implementation");

		CompletionHeader hdr = CompletionHeader::fromRaw(rc->lower(96_b));
		setName(hdr, "rc_header");
		setName(sop(rc), "rc_sop");

		UInt lastByteCount = constructFrom(hdr.byteCount);

		Bit shouldCapture = valid(rc) & sop(rc) & hdr.byteCount >= lastByteCount;

		lastByteCount = hdr.byteCount;
		ENIF(valid(rc) & sop(rc)) {
			lastByteCount = reg(lastByteCount, 0);
		}

		ENIF(valid(rc) & sop(rc) & shouldCapture) {
			hdr = reg(hdr); // captures and holds the header before it gets squished by the shift right.
		}
		setName(hdr.byteCount, "byteCount");
		auto rcPayloadStream = scl::strm::streamShiftRight(move(rc), 96);

		TileLinkChannelD d = constructFrom(*(tileLinkInit<TileLinkUL>(64_b, rcPayloadStream->width(), 0_b, sizeW).d));

		d->data = *rcPayloadStream; //cannot support smaller than full width requests.
		d->opcode = (size_t) TileLinkD::OpCode::AccessAckData;
		d->sink = 0;
		d->param = 0;
		d->error = hdr.common.poisoned | (hdr.completionStatus != (size_t) CompletionStatus::successfulCompletion);

		IF(valid(rcPayloadStream) & sop(rcPayloadStream))
			sim_assert(bitcount(hdr.byteCount) == 1) << "TileLink cannot represent non powers of 2 amount of bytes";
		UInt logByteSize = encoder((OneHot) hdr.byteCount);
		IF(valid(rcPayloadStream) & sop(rcPayloadStream))
			sim_assert(zext(logByteSize) < zext(d->size.width().count())) << "breaking this assertion invalidates the next line's truncation";
		d->size = logByteSize.lower(d->size.width());

		valid(d) = valid(rcPayloadStream);
		ready(rcPayloadStream) = ready(d);

		return d;
	}

	TileLinkUL makePciMaster(RequesterInterface&& reqInt, BitWidth byteAddressW, BitWidth dataW, BitWidth tagW)
	{
		HCL_DESIGNCHECK_HINT(tagW <= 8_b, "pcie cannot accommodate more than 8 bit tags");
		TileLinkUL ret = tileLinkInit<TileLinkUL>(byteAddressW, dataW, tagW);

		TileLinkChannelA a = constructFrom(ret.a);
		a <<= ret.a;
		
		*reqInt.request <<= tileLinkAToRequesterRequest(move(a), (*reqInt.request)->width());
		*ret.d = requesterCompletionToTileLinkD(move(reqInt.completion), byteAddressW, dataW);
	
		return ret;
	}

	TileLinkUL makePciMasterFullW(RequesterInterface&& reqInt)
	{
		TileLinkUL ret = tileLinkInit<TileLinkUL>(64_b, (*reqInt.request)->width(), 8_b);

		TileLinkChannelA a = constructFrom(ret.a);
		a <<= ret.a;

		*reqInt.request <<= tileLinkAToRequesterRequest(move(a), (*reqInt.request)->width());
		*ret.d = requesterCompletionToTileLinkDFullW(move(reqInt.completion));

		return ret;
	}

	TileLinkUB makePciMasterCheapBurst(RequesterInterface&& reqInt, std::optional<BVec> tag, std::optional<BitWidth> sizeW, BitWidth addressW) {
		Area area{ "makePciMasterCheapBurst", true };

		TileLinkUB ret = tileLinkInit<TileLinkUB>(addressW, (*reqInt.request)->width(), 0_b, sizeW);

		TileLinkChannelA a = constructFrom(ret.a);
		a <<= ret.a;


		auto rr = tileLinkAToRequesterRequest(move(a));
		if (tag)
			rr = move(rr.transform([&](const BVec& in) {
				BVec ret = in;
				IF(valid(rr) & sop(rr)) {
					auto hdr = RequestHeader::fromRaw(ret.lower(128_b));
					hdr.tag = zext(*tag);
					ret.lower(128_b) = hdr;
				}
				return ret;
			}));
	
		*reqInt.request <<= rr;
		*ret.d = requesterCompletionToTileLinkDCheapBurst(move(reqInt.completion), sizeW);

		HCL_NAMED(ret);

		return ret;
	}
}
