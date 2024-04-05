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

#include <gatery/scl_pch.h>
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

	RequesterRequestDescriptor createDescriptor(const RequestHeader& hdr)
	{
		RequesterRequestDescriptor ret;
		ret.at = hdr.common.addressType;
		ret.wordAddress = hdr.wordAddress;
		ret.dwordCount = hdr.common.dataLength();
		ret.reqType = 1; //memory write request
		IF(hdr.common.isMemRead())
			ret.reqType = 0;
		ret.poisonedReq = hdr.common.poisoned;
		ret.requesterID = hdr.requesterId;
		ret.tag = hdr.tag;
		ret.completerID = ConstBVec(ret.completerID.width());
		ret.requesterIDEnable = '0';
		ret.tc = hdr.common.trafficClass;
		ret.attr = (BVec) pack(hdr.common.attributes);
		ret.forceECRC = '0';
		return ret;
	}

	CompletionHeader createHeader(const RequesterCompletionDescriptor& desc)
	{
		CompletionHeader ret;

		ret.common.opcode(TlpOpcode::completionWithData);
		IF(desc.lockedReadCompletion) ret.common.opcode(TlpOpcode::completionforLockedMemoryReadWithData);
		ret.common.trafficClass = desc.tc;
		
		ret.common.attributes.idBasedOrdering = desc.attr[2];
		ret.common.processingHintPresence = '0';

		ret.common.digest = '0';
		ret.common.poisoned = desc.poisonedCompletion;
		ret.common.attributes.relaxedOrdering = desc.attr[1];
		ret.common.attributes.noSnoop = desc.attr[0];
		ret.common.addressType = (size_t) AddressType::defaultOption;
		ret.common.length = desc.dwordCount;

		ret.requesterId = desc.requesterId;
		ret.tag = desc.tag;
		ret.completerId = desc.completerId;
		ret.byteCount = desc.byteCount;
		ret.byteCountModifier = '0';
		ret.lowerByteAddress = desc.lowerByteAddress.lower(7_b);
		ret.completionStatus = desc.completionStatus;
		desc.requestCompleted; // maybe do something with this?
		return ret;
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
		sim_assert(emptyBits(in).lower(5_b) == 0) << __FILE__ << " " << __LINE__; // the granularity of this empty signal cannot be less than 32 bits (expect the lsb's to be completely synthesized away)
		UInt emptyWords = emptyBits(in).upper(-5_b);
		BVec throwAway = (BVec) cat('0', uintToThermometric(emptyWords));
		dwordEnable(ret) = swapEndian(~throwAway, 1_b); 

		setName(ret, "axi_out");
		return ret;
	}

	Axi4PacketStream<RQUser> requesterRequestVendorUnlocking(TlpPacketStream<scl::EmptyBits>&& in)
	{
		Area area{ "requester_request_vendor_unlocking", true };
		setName(in, "tlp_in");
		HCL_DESIGNCHECK_HINT(in->width() >= 128_b, "stream must be at least as big as 4dw for this implementation");

		RequestHeader hdr = RequestHeader::fromRaw(in->lower(128_b));

		RequesterRequestDescriptor desc = createDescriptor(hdr);

		Axi4PacketStream<RQUser> ret(in->width());
		dwordEnable(ret) = ret->width() / 32;
		ret.set(RQUser{ConstBVec(ret->width() == 512_b ? 137_b : 62_b)}); //no logic here, just documentation
		if(ret->width() == 512_b) {
			ret.template get<RQUser>().raw.lower(4_b) = hdr.firstDWByteEnable;
			ret.template get<RQUser>().raw(8, 4_b) = hdr.lastDWByteEnable;
		}
		else{
			ret.template get<RQUser>().raw.lower(4_b) = hdr.firstDWByteEnable;
			ret.template get<RQUser>().raw(4, 4_b) = hdr.lastDWByteEnable;
		}

		*ret = *in;
		IF(sop(in))
			ret->lower(128_b) = (BVec) pack(desc);

		ready(in) = ready(ret);
		valid(ret) = valid(in);
		eop(ret) = eop(in);

		//dwordEnable to empty conversion:
		IF (valid(in) & eop(in))
			sim_assert(emptyBits(in).lower(5_b) == 0) << __FILE__ << " " << __LINE__; // the granularity of this empty signal cannot be less than 32 bits (expect the lsb's to be completely synthesized away)
		UInt emptyWords = emptyBits(in).upper(-5_b);
		BVec throwAway = (BVec) cat('0', uintToThermometric(emptyWords));
		dwordEnable(ret) = swapEndian(~throwAway, 1_b); 

		setName(ret, "tlp_out");
		return ret;
	}

	TlpPacketStream<scl::EmptyBits> requesterCompletionVendorUnlocking(Axi4PacketStream<RCUser>&& in, bool straddle)
	{
		Area area{ "requester_completion_vendor_unlocking", true };
		setName(in, "tlp_in");
		HCL_DESIGNCHECK_HINT(!straddle, "not yet implemented");
		HCL_DESIGNCHECK_HINT(in->width() == 512_b, "targeting 512_b at 250MHz");

		RequesterCompletionDescriptor desc;
		unpack(in->lower(96_b), desc);

		CompletionHeader hdr = createHeader(desc);

		TlpPacketStream<scl::EmptyBits> ret(in->width());

		//data assignments
		*ret = *in;
		IF(sop(in))
			ret->lower(96_b) = (BVec) hdr;

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

	RequesterInterface requesterVendorUnlocking(Axi4PacketStream<RCUser>&& completion, Axi4PacketStream<RQUser>& request)
	{
		Area area{ "requesterInterfaceVendorUnlocking", true };
		setName(completion, "axi_st_requester_completion");
		tap(completion);

		RequesterInterface ret;
		**ret.request = completion->width();
		emptyBits(*ret.request) = BitWidth::count((*ret.request)->width().bits());

		*ret.completion = completion->width();
		emptyBits(ret.completion) = BitWidth::count(ret.completion->width().bits());

		TlpPacketStream<EmptyBits> temp = constructFrom(*ret.request);
		temp <<= *ret.request;
		request = requesterRequestVendorUnlocking(move(temp));

		setName(request, "axi_st_requester_completion");
		tap(request);

		ret.completion = requesterCompletionVendorUnlocking(move(completion));

		return ret;
	}

}

