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
		Area area{ "tlpToTileLinkA", true };
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
		Area area{ "tileLinkDToTlp", true };
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
		emptyBits(complCompl) = 512 - 4 * 32;
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
}
