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
#include "gatery/pch.h"
#include "pci.h"

#include "../Fifo.h"
#include "../stream/StreamArbiter.h"

namespace gtry::scl::pci {

	void SimTlp::makeHeader(TlpInstruction instr)
	{
		header.resize(128); //make it always be 4 dw for now
		Helper helper(header);

		helper
			.write(instr.opcode, 8_b)
			.write(instr.th, 1_b)
			.skip(1_b)
			.write(instr.idBasedOrderingAttr2, 1_b)
			.skip(1_b)
			.write(instr.tc, 3_b)
			.skip(1_b)
			.write(*instr.length >> 8, 2_b)
			.write(instr.at, 2_b)
			.write(instr.noSnoopAttr0, 1_b)
			.write(instr.relaxedOrderingAttr1, 1_b)
			.write(instr.ep, 1_b)
			.write(instr.td, 1_b)
			.write(*instr.length & 0xFF, 8_b);

		HCL_DESIGNCHECK(helper.offset == 32);

		switch (instr.opcode) {
			case TlpOpcode::memoryReadRequest64bit: {
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set");
				HCL_DESIGNCHECK_HINT(instr.address, "address not set");
				helper
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.write((*instr.address >> 56) & 0xFF, 8_b)
					.write((*instr.address >> 48) & 0xFF, 8_b)
					.write((*instr.address >> 40) & 0xFF, 8_b)
					.write((*instr.address >> 32) & 0xFF, 8_b)
					.write((*instr.address >> 24) & 0xFF, 8_b)
					.write((*instr.address >> 16) & 0xFF, 8_b)
					.write((*instr.address >> 8) & 0xFF, 8_b)
					.write((*instr.address) & 0x11111100, 6_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::memoryWriteRequest64bit:
			{
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set");
				HCL_DESIGNCHECK_HINT(instr.address, "address not set");
				helper
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.write((*instr.address >> 56) & 0xFF, 8_b)
					.write((*instr.address >> 48) & 0xFF, 8_b)
					.write((*instr.address >> 40) & 0xFF, 8_b)
					.write((*instr.address >> 32) & 0xFF, 8_b)
					.write((*instr.address >> 24) & 0xFF, 8_b)
					.write((*instr.address >> 16) & 0xFF, 8_b)
					.write((*instr.address >> 8) & 0xFF, 8_b)
					.write((*instr.address) & 0x11111100, 6_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 128);
				break;
			}
			case TlpOpcode::completionWithData:
			{
				HCL_DESIGNCHECK_HINT(instr.length, "length not set");
				HCL_DESIGNCHECK_HINT(instr.completerID, "completer id not set");
				HCL_DESIGNCHECK_HINT(instr.byteCount, "byteCount not set");
				HCL_DESIGNCHECK_HINT(instr.requesterID, "requester id not set, this should come from your data requester");
				HCL_DESIGNCHECK_HINT(instr.tag, "tag not set, it should come from your data requester");
				helper
					//START HERE
					.write(*instr.completerID >> 8, 8_b)
					.write(*instr.completerID & 0xFF, 8_b)
					.write(*instr.byteCount >> 8, 4_b)
					.write(instr.byteCountModifier, 1_b)
					.write(instr.completionStatus, 3_b)
					.write(*instr.requesterID >> 8, 8_b)
					.write(*instr.requesterID & 0xFF, 8_b)
					.write(*instr.tag, 8_b)
					.write(instr.lastDWByteEnable, 4_b)
					.write(instr.firstDWByteEnable, 4_b)
					.skip(2_b);
				HCL_DESIGNCHECK(helper.offset == 96);
				break;
			}
		}
	}
}

