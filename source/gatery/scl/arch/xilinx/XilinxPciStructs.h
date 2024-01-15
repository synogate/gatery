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
#pragma once
#include <gatery/frontend.h>

namespace gtry::scl::pci::xilinx {
	struct CQUser;
	struct CCUser;
	struct RQUser;
	struct RCUser;
	struct CompleterRequestDescriptor;
	struct CompleterCompletionDescriptor;
}

namespace gtry::scl::pci::xilinx {
	struct CompleterRequestDescriptor {
		BVec at = 2_b;
		UInt wordAddress = 62_b;
		UInt dwordCount = 11_b;
		BVec reqType = 4_b;
		Bit reservedDw2;
		BVec requesterID = 16_b;
		BVec tag = 8_b;
		BVec targetFunction = 8_b;
		BVec barId = 3_b;
		UInt barAperture = 6_b;
		BVec tc = 3_b;
		BVec attr = 3_b;
		Bit reservedDw3;
	};

	struct RequesterRequestDescriptor {
		BVec at = 2_b;
		UInt wordAddress = 62_b;
		UInt dwordCount = 11_b;
		BVec reqType = 4_b;
		Bit poisonedReq;
		BVec requesterID = 16_b;
		BVec tag = 8_b;
		BVec completerID = 16_b;
		Bit requesterIDEnable;
		BVec tc = 3_b;
		BVec attr = 3_b;
		Bit forceECRC;
	};

	struct CompleterCompletionDescriptor {
		UInt lowerByteAddress = 7_b;
		Bit reservedDw0_0;
		BVec at = 2_b;
		BVec reservedDw0_1 = 6_b;
		UInt byteCount = 13_b;
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

	struct RequesterCompletionDescriptor {
		UInt lowerByteAddress = 12_b;
		BVec errorCode = 4_b;
		UInt byteCount = 13_b;
		Bit lockedReadCompletion;
		Bit requestCompleted;
		Bit reservedDw0;
		UInt dwordCount = 11_b;
		BVec completionStatus = 3_b;
		Bit poisonedCompletion;
		Bit reservedDw1;
		BVec requesterId = 16_b;
		BVec tag = 8_b;
		BVec completerId = 16_b;
		Bit reservedDw3_88;
		BVec tc = 3_b;
		BVec attr = 3_b;
		Bit reservedDw3_95;
	};

	struct CQUser {
		BVec raw;
		static CQUser create(BitWidth streamW);

		BVec firstBeByteEnable(size_t idx = 0)	const{ return raw.width() == 512_b ? raw(0 + idx*4, 4_b) : raw(0, 4_b); }
		BVec lastBeByteEnable(size_t idx = 0)	const{ return raw.width() == 512_b ? raw(8 + idx*4, 4_b) : raw(4, 4_b); }
		Bit tphPresent(size_t idx = 0)			const{ return raw.width() == 512_b ? raw[97 + idx]		: raw[12]; }
		BVec tphType(size_t idx = 0)			const{ return raw.width() == 512_b ? raw(99 + idx*2,2_b) : raw(13,2_b); }

		void firstBeByteEnable(BVec newVal, size_t idx = 0)	{ if(raw.width() == 512_b) raw(0 + idx*4, 4_b) = newVal; else raw(0, 4_b) = newVal; }
		void lastBeByteEnable(BVec newVal, size_t idx = 0)	{ if(raw.width() == 512_b) raw(8 + idx*4, 4_b) = newVal; else raw(4, 4_b) = newVal; }
		void tphPresent(Bit newVal, size_t idx = 0)			{ if(raw.width() == 512_b) raw[97 + idx]	   = newVal; else raw[12]	  = newVal; }
		void tphType(BVec newVal, size_t idx = 0)			{ if(raw.width() == 512_b) raw(99 + idx*2,2_b) = newVal; else raw(13,2_b) = newVal; }
	};

	struct CCUser {
		BVec raw;
		static CCUser create(BitWidth streamW);
	};

	struct RQUser {
		BVec raw;
		static RQUser create(BitWidth streamW);
	};

	struct RCUser {
		BVec raw;
		static RCUser create(BitWidth streamW);
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CompleterRequestDescriptor, at, wordAddress, dwordCount, reqType, reservedDw2, requesterID, tag, targetFunction, barId, barAperture, tc, attr, reservedDw3);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CompleterCompletionDescriptor, lowerByteAddress, reservedDw0_0, at, reservedDw0_1, byteCount, lockedReadCompletion, reservedDw0_3, dwordCount, completionStatus, poisonedCompletion, reservedDw1, requesterID, tag, completerID, completerIdEnable, tc, attr, forceECRC);

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RequesterRequestDescriptor, at, wordAddress, dwordCount, reqType, poisonedReq, requesterID, tag, completerID, requesterIDEnable, tc, attr, forceECRC);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RequesterCompletionDescriptor, lowerByteAddress, errorCode, byteCount, lockedReadCompletion, requestCompleted, reservedDw0, dwordCount, completionStatus, poisonedCompletion, reservedDw1, requesterId, tag, completerId, reservedDw3_88, tc, attr, reservedDw3_95);

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CQUser, raw);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CCUser, raw);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RQUser, raw);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RCUser, raw);
