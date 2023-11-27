/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include <gatery/scl/utils/Thermometric.h>
#include "XilinxPci.h"

namespace gtry::scl::pci::xilinx {

	RequestHeader createHeader(const CompleterRequestDescriptor& desc, const CQUser& cqUser)
	{
		RequestHeader hdr;

		hdr.common.fmt = 0b001;

		Bit isWrite = desc.reqType == 0b0001;
		IF(isWrite) hdr.common.fmt = 0b011;

		hdr.common.type = 0b0'0000;
		hdr.common.trafficClass = desc.tc;
		hdr.common.attributes.idBasedOrdering = desc.attr[2];
		hdr.common.processingHintPresence = cqUser.tphPresent();

		hdr.common.digest = '0';
		hdr.common.poisoned = '0';
		hdr.common.attributes.relaxedOrdering = desc.attr[1];
		hdr.common.attributes.noSnoop = desc.attr[0];
		hdr.common.addressType = desc.at;
		hdr.common.length = desc.dwordCount;

		hdr.firstDWByteEnable = cqUser.firstBeByteEnable();
		hdr.lastDWByteEnable = cqUser.lastBeByteEnable();
		hdr.tag = desc.tag;
		hdr.requesterId = desc.requesterID;

		hdr.wordAddress = desc.wordAddress;
		hdr.processingHint = cqUser.tphType();

		return hdr;
	}

	CompleterCompletionDescriptor createDescriptor(const CompletionHeader& hdr) {
		CompleterCompletionDescriptor desc;
		desc.lowerByteAddress = hdr.lowerByteAddress;
		desc.at = hdr.common.addressType;
		desc.byteCount = zext(hdr.byteCount);
		desc.lockedReadCompletion = hdr.common.type == 0b01011;
		desc.dwordCount =  hdr.common.dataLength();
		desc.completionStatus = hdr.completionStatus;
		desc.poisonedCompletion = hdr.common.poisoned;
		desc.requesterID = hdr.requesterId;
		desc.tag = hdr.tag;
		desc.completerID = hdr.completerId;
		desc.completerIdEnable = '0';
		desc.tc = hdr.common.trafficClass;
		desc.attr = (BVec) pack(hdr.common.attributes);
		desc.forceECRC = '0';
		return desc;
	}

	TlpPacketStream<scl::EmptyBits, pci::BarInfo> completerRequestVendorUnlocking(Axi4PacketStream<CQUser>&& in)
	{
		Area area{ "completer_request_vendor_unlocking", true };
		setName(in, "axi_in");
		/*
		* since we will work with 512 bit dw, we will definitely receive the entire descriptor within the first beat:
		* the parsing and translating should hence be quite simple
		*/
		HCL_DESIGNCHECK_HINT(in->width() >= 128_b, "stream must be at least as big as 4dw for this implementation");
		CompleterRequestDescriptor desc;
		unpack(in->lower(128_b), desc);

		RequestHeader hdr = createHeader(desc, in.get<CQUser>());

		TlpPacketStream<scl::EmptyBits, BarInfo> ret(in->width());

		//data assignments
		*ret = *in;
		IF(sop(in))
			ret->lower(128_b) = (BVec) hdr;
		ret.set(BarInfo{ .id = desc.barId, .logByteAperture = desc.barAperture });

		//Handshake assignments
		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//dwordEnable to empty conversion:
		UInt emptyWords = thermometricToUInt(~dwordEnable(in)).lower(-1_b); HCL_NAMED(emptyWords);
		emptyBits(ret) = cat(emptyWords, "5b0");

		setName(ret, "tlp_out");
		return ret;
	}

	Axi4PacketStream<CCUser> completerCompletionVendorUnlocking(TlpPacketStream<EmptyBits>&& in)
	{
		Area area{ "completer_completion_vendor_unlocking", true };
		setName(in, "tlp_in");
		HCL_DESIGNCHECK_HINT(in->width() >= 96_b, "stream must be at least as big as 3dw for this implementation");

		CompletionHeader hdr = CompletionHeader::fromRaw(in->lower(96_b));
		CompleterCompletionDescriptor desc = createDescriptor(hdr);

		Axi4PacketStream<CCUser> ret(in->width());
		CCUser ccUser = CCUser::create(in->width());
		ccUser.raw = 0;
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
		BVec throwAway = (BVec) cat('0', uintToThermometric(emptyWords));
		dwordEnable(ret) = swapEndian(~throwAway, 1_b); 

		setName(ret, "axi_out");
		return ret;
	}

}

