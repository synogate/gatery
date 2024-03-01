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
#pragma once
#include <gatery/frontend.h>

#include <gatery/scl/stream/Stream.h>

namespace gtry::scl::pci
{
	template <Signal ...Meta>
	using TlpPacketStream = RvPacketStream<BVec, Meta...>;

	enum class TlpOpcode;
	enum class CompletionStatus;
	enum class AddressType;
	enum class ProcessingHint;
	enum class TrafficClass;
	struct Attributes;
	struct HeaderCommon;
	struct CompletionHeader;
	struct RequestHeader;
	struct BarInfo;
	struct CompleterInterface;

}



namespace gtry::scl::pci
{
	enum class TlpOpcode {
		memoryReadRequest32bit							= 0b000'0'0000,	//MRd32
		memoryReadRequest64bit							= 0b001'0'0000,	//MRd64 
		memoryReadRequestLocked32bit					= 0b000'0'0001,	//MRdLk32
		memoryReadRequestLocked64bit					= 0b001'0'0001,	//MRdLk64
		memoryWriteRequest32bit							= 0b010'0'0000,	//MWr
		memoryWriteRequest64bit							= 0b011'0'0000,	//MWr64
		ioReadRequest									= 0b000'0'0010,	//IORd
		ioWriteRequest									= 0b010'0'0010,	//IOWr
		configurationReadType0							= 0b000'0'0100,	//CfgRd0
		configurationWriteType0							= 0b010'0'0100,	//CfgWr0
		configurationReadType1							= 0b000'0'0101,	//CfgRd1
		configurationWriteType1							= 0b010'0'0101,	//CfgWr1
		messageRequest									= 0b001'1'0000,	//Msg
		messageRequestWithDataPayload					= 0b011'1'0000,	//MsgD
		completionWithoutData							= 0b000'0'1010,	//Cpl
		completionWithData								= 0b010'0'1010,	//CplD 
		completionforLockedMemoryReadWithoutData		= 0b000'0'1011,	//CplLk
		completionforLockedMemoryReadWithData			= 0b010'0'1011,	//CplDLk
		fetchAndAddAtomicOpRequest32bit					= 0b010'0'1100,	//FetchAdd32
		fetchAndAddAtomicOpRequest64bit					= 0b011'0'1100,	//FetchAdd64
		unconditionalSwapAtomicOpRequest32bit			= 0b010'0'1101,	//Swap32
		unconditionalSwapAtomicOpRequest64bit			= 0b011'0'1101,	//Swap64
		compareAndSwapAtomicOpRequest32bit				= 0b010'0'1110,	//CAS32
		compareAndSwapAtomicOpRequest64bit				= 0b011'0'1110,	//CAS64
		other															//error or unimplemented
	};

	enum class CompletionStatus {
		defaultOption				= 0b000, 
		successfulCompletion		= 0b000, //SC
		unsupportedRequest			= 0b001, //UR
		configRequestRetryStatus	= 0b010, //CRS
		completerAbort				= 0b100, //CA
	};

	enum class AddressType {
		defaultOption			= 0b00,
		untranslated			= 0b00,
		translationRequest		= 0b01,
		translatedRequest		= 0b10,
		reserved				= 0b11,
	};

	enum class ProcessingHint {
		defaultOption				= 0b00,
		bidirectionalDataStructure	= 0b00,
		requester					= 0b01,
		target						= 0b10,
		targetWithPriority			= 0b11,
	};

	enum class TrafficClass {
		defaultOption	= 0,
		bestEffort	= 0,
		tc0	= 0,
		tc1	= 1,
		tc2	= 2,
		tc3	= 3,
		tc4	= 4,
		tc5	= 5,
		tc6	= 6,
		tc7	= 7
	};

	struct Attributes {
		Bit noSnoop;
		Bit relaxedOrdering;	
		Bit idBasedOrdering; 	
		static Attributes createDefault(){ return { '0','0','0' }; }
	};

	struct HeaderCommon
	{
		BVec rawDw0();
		static HeaderCommon fromRawDw0(BVec rawDw0);
		static HeaderCommon makeDefault(TlpOpcode opcode, const UInt& length);

		Bit poisoned;
		Bit digest;
		
		Bit processingHintPresence;	
		Attributes attributes;

		BVec addressType = 2_b;
		BVec trafficClass = 3_b;

		BVec fmt = 3_b;		
		BVec type = 5_b;		//type and format
		UInt length = 10_b;		//request payload length

		UInt dataLength() const { UInt ret = 1024; IF(length != 0) ret = zext(length); return ret; }
		void dataLength(UInt len) { length = len.lower(10_b); }
		
		void opcode(TlpOpcode op) { fmt = (size_t)op >> 5; type = (size_t) op & 0x1F; }
	
		Bit is3dw()			const { return fmt == 0b000 | fmt == 0b010; }
		Bit is4dw()			const { return !is3dw(); }
		Bit hasData()		const { return fmt == 0b010 | fmt == 0b011; }
							
		Bit isCompletion()	const { return type == 0b01010; }
		Bit isMemRW()		const { return type == 0b00000 | type == 0b00001; }
		Bit isMemWrite()	const { return fmt.upper(2_b) == 0b01; }
		Bit isMemRead()		const { return fmt.upper(2_b) == 0b00; }
		UInt hdrSizeInDw()	const { UInt ret = 4; IF(is3dw()) ret = 3; return ret; }

	};

	struct CompletionHeader
	{
		[[nodiscard]] static CompletionHeader fromRaw(BVec rawHeader);
		operator BVec();

		HeaderCommon common;

		BVec requesterId = 16_b;
		BVec tag = 8_b;
		BVec completerId = 16_b;
		UInt byteCount = 12_b;
		Bit byteCountModifier;
		UInt lowerByteAddress = 7_b;
		BVec completionStatus = 2_b;
	};

	struct RequestHeader
	{
		[[nodiscard]] static RequestHeader fromRaw(BVec rawHeader);
		static RequestHeader makeWriteDefault(const UInt& wordAddress, const UInt& length, const BVec& tag);
		operator BVec();

		HeaderCommon common;

		BVec requesterId = 16_b;
		BVec tag = 8_b;
		BVec lastDWByteEnable = 4_b;
		BVec firstDWByteEnable = 4_b;
		UInt wordAddress = 62_b;
		BVec processingHint = 2_b;

	};

	struct BarInfo {
		BVec id = 3_b;
		UInt logByteAperture = 6_b; //0-> 1B | 10 -> 1kB | 20 -> 1MB | 30 -> 1GB | etc.
	};

	struct CompleterInterface {
		TlpPacketStream<EmptyBits, BarInfo> request;
		TlpPacketStream<EmptyBits> completion;
	};

	struct RequesterInterface {
		Reverse<TlpPacketStream<EmptyBits>> request;
		TlpPacketStream<EmptyBits> completion;
	};

	RequesterInterface simOverrideReqInt(RequesterInterface &&hardware, RequesterInterface &&simulation);
}
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::CompleterInterface, request, completion);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::RequesterInterface, request, completion);

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::HeaderCommon, poisoned, digest, processingHintPresence, attributes, addressType, trafficClass, fmt, type, length);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::RequestHeader, common, requesterId, tag, lastDWByteEnable, firstDWByteEnable, wordAddress, processingHint);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::CompletionHeader, common, requesterId, tag, completerId, byteCount, byteCountModifier, lowerByteAddress, completionStatus);



