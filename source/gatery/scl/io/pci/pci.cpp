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
#include "gatery/scl_pch.h"
#include "pci.h"



namespace gtry::scl::pci {

	HeaderCommon HeaderCommon::makeDefault(TlpOpcode opcode, const UInt& length)
	{
		HeaderCommon ret;

		ret.poisoned = '0';
		ret.digest = '0';
		ret.processingHintPresence = '0';
		ret.attributes = pci::Attributes::createDefault();
		ret.addressType = (size_t) pci::AddressType::defaultOption;
		ret.trafficClass = (size_t) pci::TrafficClass::defaultOption;
		ret.opcode(opcode);
		ret.length = zext(length);

		return ret;
	}

	HeaderCommon HeaderCommon::fromRawDw0(BVec rawDw0)
	{
		HCL_DESIGNCHECK(rawDw0.width() >= 32_b);

		HeaderCommon ret;
		auto bytes = rawDw0.parts(4);
		ret.type = bytes[0](0, 5_b);
		ret.fmt = bytes[0](5, 3_b);
		ret.processingHintPresence = bytes[1][0];
		ret.trafficClass = bytes[1](4, 3_b);
		ret.addressType = bytes[2](2, 2_b);
		ret.attributes.idBasedOrdering = bytes[1][2];
		ret.attributes.relaxedOrdering = bytes[2][5];
		ret.attributes.noSnoop = bytes[2][4];
		ret.poisoned = bytes[2][6];
		ret.digest = bytes[2][7];
		ret.length = cat(bytes[2](0, 2_b), bytes[3]);

		return ret;
	}

	BVec HeaderCommon::rawDw0()
	{
		BVec dw0 = ConstBVec(32_b);
		auto bytes = dw0.parts(4);

		bytes[0].lower(5_b) = type;
		bytes[0].upper(3_b) = fmt;

		bytes[1] = 0;
		bytes[1][0] = processingHintPresence;
		bytes[1][2] = attributes.idBasedOrdering;
		bytes[1](4, 3_b) = trafficClass;
		bytes[2].lower(2_b) = (BVec) length.upper(2_b);
		bytes[2](2, 2_b) = addressType;
		bytes[2][4] = attributes.noSnoop;
		bytes[2][5] = attributes.relaxedOrdering;
		bytes[2][6] = poisoned;
		bytes[2][7] = digest;
		bytes[3] = (BVec) length.lower(8_b);

		return dw0;
	}

	RequestHeader RequestHeader::makeWriteDefault(const UInt& wordAddress, const UInt& length, const BVec& tag)
	{
		RequestHeader ret;

		ret.common = HeaderCommon::makeDefault(TlpOpcode::memoryWriteRequest64bit, length);
		ret.firstDWByteEnable = 0xF;
		ret.lastDWByteEnable = 0xF;

		ret.processingHint = (size_t) pci::ProcessingHint::defaultOption;
		ret.requesterId = 0;
		ret.tag = zext(tag);

		HCL_DESIGNCHECK(wordAddress.width() <= 62_b);
		ret.wordAddress = zext(wordAddress);
		return ret;
	}

	RequestHeader RequestHeader::fromRaw(BVec rawHeader)
	{
		HCL_DESIGNCHECK_HINT(rawHeader.width() == 128_b, "A request header should have 4 DW");
		RequestHeader ret;

		ret.common = HeaderCommon::fromRawDw0(rawHeader.part(4, 0));

		auto dw = rawHeader.parts(4);
		auto dw1Bytes = dw[1].parts(4);

		ret.requesterId = (BVec) cat(dw1Bytes[0], dw1Bytes[1]);
		ret.tag = dw1Bytes[2];
		ret.lastDWByteEnable = dw1Bytes[3].upper(4_b);
		ret.firstDWByteEnable = dw1Bytes[3].lower(4_b);
		
		ret.wordAddress = ConstUInt(62_b);
		ret.wordAddress.upper(32_b) = (UInt) swapEndian(dw[2], 8_b);
		ret.wordAddress.lower(30_b) = (UInt) swapEndian(dw[3], 8_b).upper(30_b);
		ret.processingHint = dw[3].part(4, 3).lower(2_b);
		return ret;
	}

	RequestHeader::operator BVec() 
	{
		BVec ret = ConstBVec(128_b);

		auto dw = ret.parts(4);
		dw[0] = common.rawDw0();

		auto dw1Bytes = dw[1].parts(4);

		dw1Bytes[0] = requesterId.upper(8_b);
		dw1Bytes[1] = requesterId.lower(8_b);
		dw1Bytes[2] = tag;
		dw1Bytes[3].upper(4_b) = lastDWByteEnable;
		dw1Bytes[3].lower(4_b) = firstDWByteEnable;

		dw[2] = (BVec) swapEndian(wordAddress.upper(32_b), 8_b);
		dw[3] = swapEndian( (BVec) cat(wordAddress.lower(30_b), processingHint), 8_b);

		return ret;
	}

	CompletionHeader CompletionHeader::fromRaw(BVec rawHeader)
	{
		HCL_DESIGNCHECK_HINT(rawHeader.width() == 96_b, "A completion header should have 3 DW");

		CompletionHeader ret;
		ret.common = HeaderCommon::fromRawDw0(rawHeader.part(3, 0));

		auto dw = rawHeader.parts(3);
		auto dw1Bytes = dw[1].parts(4);

		ret.completerId = (BVec)cat(dw1Bytes[0], dw1Bytes[1]);
		ret.completionStatus = dw1Bytes[2](5, 3_b);
		ret.byteCountModifier = dw1Bytes[2][4];
		ret.byteCount = cat(dw1Bytes[2](0, 4_b), dw1Bytes[3]);

		auto dw2Bytes = dw[2].parts(4);

		ret.requesterId = (BVec)cat(dw2Bytes[0], dw2Bytes[1]);
		ret.tag = dw2Bytes[2];
		ret.lowerByteAddress = (UInt) dw2Bytes[3].lower(7_b);

		return ret;
	}

	CompletionHeader::operator BVec()
	{
		BVec ret = ConstBVec(96_b);

		auto dw = ret.parts(3);
		dw[0] = common.rawDw0();

		auto dw1Bytes = dw[1].parts(4);

		dw1Bytes[0] = completerId.upper(8_b);
		dw1Bytes[1] = completerId.lower(8_b);

		dw1Bytes[2].lower(4_b) = (BVec) byteCount.upper(4_b);
		dw1Bytes[2][4] = byteCountModifier;
		dw1Bytes[2].upper(3_b) = completionStatus;
		dw1Bytes[3] = (BVec) byteCount.lower(8_b);

		auto dw2Bytes = dw[2].parts(4);

		dw2Bytes[0] = requesterId.upper(8_b);
		dw2Bytes[1] = requesterId.lower(8_b);
		dw2Bytes[2] = tag;
		dw2Bytes[3].msb() = '1';
		dw2Bytes[3].lower(7_b) = (BVec) lowerByteAddress;
		
		return ret;
	}
	
	RequesterInterface simOverrideReqInt(RequesterInterface &&hardware, RequesterInterface &&simulation)
	{
		RequesterInterface result = constructFrom(hardware);

		result.completion <<= simOverrideDownstream<TlpPacketStream<EmptyBits>>(move(hardware.completion), move(simulation.completion));

		auto [requestToHardware, requestToSimulation] = simOverrideUpstream<TlpPacketStream<EmptyBits>>(move(*result.request));

		*hardware.request <<= requestToHardware;
		*simulation.request <<= requestToSimulation;

		return result;
	}
}
