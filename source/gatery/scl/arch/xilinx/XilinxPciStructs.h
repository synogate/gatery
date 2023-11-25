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
	struct RQExtras;
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

	struct CQUser {
		BVec first_be = 8_b;
		BVec last_be = 8_b;
		BVec byte_en = 64_b;

		BVec is_sop = 2_b;
		BVec is_sop0_ptr = 2_b;
		BVec is_sop1_ptr = 2_b;

		BVec is_eop = 2_b;
		BVec is_eop0_ptr = 4_b;
		BVec is_eop1_ptr = 4_b;

		Bit discontinue;

		BVec tph_present = 2_b;
		BVec tph_type = 4_b;

		BVec tph_st_tag = 16_b;
		BVec parity = 64_b;
	};

	struct CCUser {
		BVec is_sop = 2_b;
		BVec is_sop0_ptr = 2_b;
		BVec is_sop1_ptr = 2_b;

		BVec is_eop = 2_b;
		BVec is_eop0_ptr = 4_b;
		BVec is_eop1_ptr = 4_b;

		Bit discontinue;

		BVec parity = 64_b;
	};


	struct RQUser {
		BVec first_be = 8_b;
		BVec last_be = 8_b;

		BVec addr_offset = 4_b;

		BVec is_sop = 2_b;
		BVec is_sop0_ptr = 2_b;
		BVec is_sop1_ptr = 2_b;

		BVec is_eop = 2_b;
		BVec is_eop0_ptr = 4_b;
		BVec is_eop1_ptr = 4_b;

		Bit discontinue;

		BVec tph_present = 2_b;
		BVec tph_type = 4_b;
		BVec tph_indirect_tag_en = 2_b;
		BVec tph_st_tag = 16_b;

		BVec seq_num0 = 6_b;
		BVec seq_num1 = 6_b;

		BVec parity = 64_b;
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
		BVec byte_en = 64_b;

		BVec is_sop = 4_b;
		BVec is_sop0_ptr = 2_b;
		BVec is_sop1_ptr = 2_b;
		BVec is_sop2_ptr = 2_b;
		BVec is_sop3_ptr = 2_b;

		BVec is_eop = 4_b;
		BVec is_eop0_ptr = 4_b;
		BVec is_eop1_ptr = 4_b;
		BVec is_eop2_ptr = 4_b;
		BVec is_eop3_ptr = 4_b;

		Bit discontinue;

		BVec parity = 64_b;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CompleterRequestDescriptor, at, wordAddress, dwordCount, reqType, reservedDw2, requesterID, tag, targetFunction, barId, barAperture, tc, attr, reservedDw3);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CompleterCompletionDescriptor, lowerByteAddress, reservedDw0_0, at, reservedDw0_1, byteCount, lockedReadCompletion, reservedDw0_3, dwordCount, completionStatus, poisonedCompletion, reservedDw1, requesterID, tag, completerID, completerIdEnable, tc, attr, forceECRC);

BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CQUser, first_be, last_be, byte_en, is_sop, is_sop0_ptr, is_sop1_ptr, is_eop, is_eop0_ptr, is_eop1_ptr, discontinue, tph_present, tph_type, tph_st_tag, parity);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::CCUser, is_sop, is_sop0_ptr, is_sop1_ptr, is_eop, is_eop0_ptr, is_eop1_ptr, discontinue, parity);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RQUser, first_be, last_be, addr_offset, is_sop, is_sop0_ptr, is_sop1_ptr, is_eop, is_eop0_ptr, is_eop1_ptr, discontinue, tph_present, tph_type, tph_indirect_tag_en, tph_st_tag, seq_num0, seq_num1, parity);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RQExtras, tag_vld0, tag_vld1, tag0, tag1, seq_num0, seq_num1, seq_num_vld0, seq_num_vld1);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::pci::xilinx::RCUser, byte_en, is_sop, is_sop0_ptr, is_sop1_ptr, is_sop2_ptr, is_sop3_ptr, is_eop, is_eop0_ptr, is_eop1_ptr, is_eop2_ptr, is_eop3_ptr, discontinue, parity);



