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
#include "../Avalon.h"
#include "../stream/Packet.h"
#include "../stream/SimuHelpers.h"

#include <gatery/scl/tilelink/tilelink.h>


namespace gtry::scl::pci
{
	struct Prefix {
		BVec prefix = 32_b;
	};

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
		//messageRequest								= 0b001'1'0000,	//Msg
		//messageRequestWithDataPayload					= 0b011'1'0000,	//MsgD
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

	struct TlpInstruction {
		TlpOpcode opcode;
		bool	th = 0;						//presence of TPH (processing hint)
		bool	idBasedOrderingAttr2 = 0;	//Attribute[2]
		uint8_t	tc = 0b000;					//traffic class
		std::optional<size_t> length;		//length msb
		size_t at = 0b00;					//Address type
		bool	noSnoopAttr0 = 0;
		bool	relaxedOrderingAttr1 = 0;
		bool	ep;							//indicator of poisoned tlp
		bool	td;							//tlp digest


		std::optional<size_t> requesterID;
		std::optional<uint8_t> tag;
		size_t firstDWByteEnable = 0b1111;
		size_t lastDWByteEnable = 0b1111;
		std::optional<uint64_t> address;
		std::optional<size_t> completerID;
		size_t completionStatus = 0b000;
		bool byteCountModifier = 0;
		std::optional<size_t> byteCount;


		//make a function that casts this struct to a SimPacket. this will be made by using a function that transforms this struct into a defaultbitvectorstate. we need a package of helpers to make a defaultbitvectorstate out of elementary things

	};

	struct Support {
		bool lockedOPs	= false;
		bool ioOps		= false;
		bool configOps	= false;
		bool atomicOps	= false;
		bool messageOps = false;
		bool addressing64bit = false;
		bool addressing32bit = false;
	};



	struct Header {
		struct DW0 {
			BVec type = 5_b;		//type
			BVec fmt = 3_b;			//format
			Bit th;					//presence of TPH (processing hint)
			Bit reservedByte1Bit1;
			Bit idBasedOrderingAttr2;			//Attribute[2]
			Bit reservedByte1Bit3;
			BVec tc = 3_b;			//traffic class
			Bit reservedByte1Bit7;
			BVec lengthMsb = 2_b;
			BVec at = 2_b;			//Address type
			Bit noSnoopAttr0;
			Bit relaxedOrderingAttr1;
			Bit ep;					//indicator of poisoned tlp
			Bit td;					//tlp digest
			BVec lengthLsb = 8_b;

			BVec length() { return (BVec) cat(lengthMsb, lengthLsb); }
			void length(BVec len) { cat(lengthMsb, lengthLsb) = (UInt) len; }
		};
		struct DW1 {
			BVec dw = 32_b;
		};
		struct DW2 {
			BVec dw = 32_b;
		};
		struct DW3 {
			BVec dw = 32_b;
		};
	
		DW0 dw0;
		DW1 dw1;
		DW2 dw2;
		DW3 dw3;

		Bit is3dw()		{ return dw0.fmt == 0b000 | dw0.fmt == 0b010; }
		Bit is4dw()		{ return !is3dw(); }
		Bit hasData()	{ return dw0.fmt == 0b010 | dw0.fmt == 0b011; }

		Bit isCompletion()	{ return dw0.type == 0b01010; }
		Bit isMemRW()		{ return dw0.type == 0b00000 | dw0.type == 0b00001; }
		UInt dataSize()		{ UInt ret = 1024; IF(dw0.length() != 0) ret = (UInt)zext(dw0.length()); return ret; }
		UInt hdrSizeInDw()		{ UInt ret = 4; IF(is3dw()) ret = 3; return ret; }
	};

	struct Tlp {
		Prefix prefix;  // 0 or 1 dw
		Header header;  // 3 or 4 dw
		BVec data;		// 0 to 1023 dw
	};

	//A TLP packet stream is a packet stream whose payload represents a TLP (transaction layer protocol) and each beat contains 1 
	// or more dw's ( 1 double word in pcie jargon means 32 bits). 
	template <Signal ...Meta>
	using TlpPacketStream = RvPacketStream<BVec, Meta...>;

	//A TLP intel packet stream consists of a header, prefix, and data. The header and prefix are synchronous and coincident with the Sop
	//The Data is synchronous with the valid signal
	template<Signal ...Meta>
	using TlpIntelPacketStream = RvPacketStream<BVec, Header, Prefix, Meta...>;

	template<Signal ...Meta>
	TlpPacketStream<Meta...> toIntelTlpPacketStream(TlpIntelPacketStream<Meta...>&& in);

	template<Signal ...Meta>
	TlpIntelPacketStream<Meta...> toTlpPacketStream(TlpPacketStream<Meta...>&& in);
	
	struct Helper {
		sim::DefaultBitVectorState &destination;
		size_t offset = 0;
		Helper(sim::DefaultBitVectorState &destination) : destination(destination) {}

		template<typename T>
		Helper& write(const T& value, BitWidth size) {
			HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "not enough space in destination to write value");
			destination.insert(sim::DefaultConfig::VALUE, offset, size.value, (sim::DefaultConfig::BaseType) value);
			destination.setRange(sim::DefaultConfig::DEFINED, offset, size.value);
			offset += size.value;
			return *this;
		}

		Helper& skip(BitWidth size) {
			HCL_DESIGNCHECK_HINT(offset + size.value <= destination.size(), "offset would overflow after this operation. This is not allowed");
			offset += size.value;
			return *this;
		}
	};



	struct SimTlp : public scl::strm::SimPacket{
		sim::DefaultBitVectorState header;
		void makeHeader(TlpInstruction instr);
	};

	template<StreamSignal StreamT>
	SimProcess sendTlp(const StreamT& stream, SimTlp tlp, Clock clk, scl::SimulationSequencer& sequencer);
}


namespace gtry::scl::pci {
	TileLinkUL makeTileLinkMaster(TlpPacketStream<EmptyBits>&& complReq, TlpPacketStream<EmptyBits>& complCompl);

	template<Signal ...Meta>
	TlpPacketStream<Meta...> toTlpPacketStream(TlpIntelPacketStream<Meta...>&& in) 
	{
		HCL_DESIGNCHECK_HINT(false, "this design is untested, do not use it");
		BitWidth dataW = in->width();

		StreamBroadcaster<TlpIntelPacketStream<Meta...>> bCaster(move(in));

		//remove the possible empty field signaling the emptiness of the data in tlp's
		RvPacketStream<Header, Meta...> hdrStream(strm::extractMeta<Header>(move(bCaster.bcastTo().template remove<Empty>().template remove<Prefix>())));

		Bit hasData = hdrStream->hasData();
		Bit is3dw = hdrStream->is3dw();

		//add empty bytes field to this so that we can extend the stream width
		BitWidth headerW = pack(*hdrStream).width();
		UInt headerEmpty = ConstUInt(0, BitWidth::count(headerW.bytes()));
		IF(hdrStream->is3dw()) headerEmpty = 4;
		hdrStream.add(Empty{ headerEmpty });

		RvPacketStream<BVec, Meta...> hdrStreamAsBVec(scl::strm::rawPayload(move(hdrStream)));
		RvPacketStream<BVec, Meta...> extendedHdrStream(scl::strm::widthExtend(move(hdrStreamAsBVec), dataW));

		UInt bitOffset = 128;
		IF(is3dw) bitOffset = 96;
		RvStream<UInt> offset(128);
		valid(offset) = flagInstantSet(valid(extendedHdrStream) & hasData, valid(extendedHdrStream) & eop(extendedHdrStream), '0');
		
		Stream dataPacketStream(bCaster.bcastTo()); /*must remove all the extra meta for cleanliness*/

		ready(dataPacketStream) |= !hasData; //if there is no data, we must assert ready on this stream so that the broadcaster allows stream popping

		auto ret(scl::strm::insert(move(extendedHdrStream), move(dataPacketStream), move(offset)));

		return ret.template reduceTo<TlpPacketStream<Meta...>>();
	}


	template<Signal ...Meta>
	TlpIntelPacketStream<Meta...> toIntelTlpPacketStream(TlpPacketStream<Meta...>&& in) 
	{
		HCL_DESIGNCHECK_HINT(false, "this design is unfinished, do not use it");
		TlpIntelPacketStream<Meta...> ret;

		ret.template get<Header>() = in.lower(pack(Header{}).width());
		*ret = *in;

		ENIF(valid & sop(in)) ret.template get<Header>() = reg(ret.template get<Header>());

		valid(ret) = valid(in) & !sop(in);
		ready(in) = ready(ret);

		/* add support for other kinds of metadata*/
		return {};
	}








	namespace amd {
		struct CQUser {
			BVec parity = 64_b;

			BVec tph_st_tag = 16_b;
			BVec tph_type = 4_b;
			BVec tph_present = 2_b;

			Bit discontinue;

			BVec is_eop1_ptr = 4_b;
			BVec is_eop0_ptr = 4_b;
			BVec is_eop = 2_b;

			BVec is_sop1_ptr = 2_b;
			BVec is_sop0_ptr = 2_b;
			BVec is_sop = 2_b;

			BVec byte_en = 64_b;
			BVec last_be = 8_b;
			BVec first_be = 8_b;
		};

		struct CCUser {
			BVec parity = 64_b;

			Bit discontinue;

			BVec is_eop1_ptr = 4_b;
			BVec is_eop0_ptr = 4_b;
			BVec is_eop = 2_b;

			BVec is_sop1_ptr = 2_b;
			BVec is_sop0_ptr = 2_b;
			BVec is_sop = 2_b;
		};

		struct RQUser {
			BVec parity = 64_b;

			BVec seq_num1 = 6_b;
			BVec seq_num0 = 6_b;

			BVec tph_st_tag = 16_b;
			BVec tph_indirect_tag_en = 2_b;
			BVec tph_type = 4_b;
			BVec tph_present = 2_b;

			Bit discontinue;

			BVec is_eop1_ptr = 4_b;
			BVec is_eop0_ptr = 4_b;
			BVec is_eop = 2_b;

			BVec is_sop1_ptr = 2_b;
			BVec is_sop0_ptr = 2_b;
			BVec is_sop = 2_b;

			BVec addr_offset = 4_b;

			BVec last_be = 8_b;
			BVec first_be = 8_b;
		};

		struct RQExtras {
			Bit tag_vld0;
			Bit tag_vld1;

			BVec tag0 = 8_b;
			BVec tag1 = 8_b;

			BVec seq_num0 = 6_b;
			BVec seq_num1 = 6_b;

			Bit seq_num_vld0;
			Bit seq_num_vld1;
		};

		struct RCUser {
			BVec parity = 64_b;

			Bit discontinue;

			BVec is_eop3_ptr = 4_b;
			BVec is_eop2_ptr = 4_b;
			BVec is_eop1_ptr = 4_b;
			BVec is_eop0_ptr = 4_b;
			BVec is_eop = 4_b;

			BVec is_sop3_ptr = 2_b;
			BVec is_sop2_ptr = 2_b;
			BVec is_sop1_ptr = 2_b;
			BVec is_sop0_ptr = 2_b;
			BVec is_sop = 4_b;

			BVec byte_en = 64_b;
		};


		/**
		* @brief amd axi4 generic packet stream
		*/
		template<Signal ...Meta>
		using axi4PacketStream = RvPacketStream<BVec, Keep, Meta...>;

		/**
		 * @brief function to convert between amd's proprietary completer request packet format to a standard TLPPacketStream
		 * @toImprove:	add support for 3dw tlp creation, at the moment we generate only 4dw tlps, even when dealing with 32 bit
		 *				requests, which is non-conformant with native tlps.
		*/
		TlpPacketStream<scl::EmptyBits>  completerRequestVendorUnlocking(axi4PacketStream<CQUser> in);
		
		/**
		 * @brief:
		*/
		axi4PacketStream<CCUser> completerCompletionVendorUnlocking(TlpPacketStream<EmptyBits> in);

	}

}
