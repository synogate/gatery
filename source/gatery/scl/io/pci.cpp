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

//gtry::Bit gtry::scl::pci::isCompletionTlp(const BVec& tlpHeader)
//{
//	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 8_b, "first 8b of tlp header required for decoding");
//	Bit completion_tlp = tlpHeader(0, 5_b) == "b01010";
//	HCL_NAMED(completion_tlp);
//	return completion_tlp;
//}
//
//gtry::Bit gtry::scl::pci::isMemTlp(const BVec& tlpHeader)
//{
//	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 8_b, "first 8b of tlp header required for decoding");
//	Bit mem_tlp = tlpHeader(0, 5_b) == 0;
//	HCL_NAMED(mem_tlp);
//	return mem_tlp;
//}
//
//gtry::Bit gtry::scl::pci::isDataTlp(const BVec& tlpHeader)
//{
//	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 32_b, "first word of tlp header required for decoding");
//	Bit data_tlp = tlpHeader[30];
//	HCL_NAMED(data_tlp);
//	return data_tlp;
//}

//gtry::scl::pci::AvmmBridge::AvmmBridge(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId) :
//	AvmmBridge()
//{
//	setup(rx, avmm, cplId);
//}
//
//void gtry::scl::pci::AvmmBridge::setup(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId)
//{
//	HCL_DESIGNCHECK_HINT(rx->header.width() == 96_b, "reduce tlp header address using discardHighAddressBits");
//	m_cplId = cplId;
//	generateFifoBridge(rx, avmm);
//}
//
//void gtry::scl::pci::AvmmBridge::generateFifoBridge(RvStream<Tlp>& rx, AvalonMM& avmm)
//{
//	HCL_DESIGNCHECK(avmm.readData->width() == 32_b);
//
//	auto entity = Area{ "AvmmBridge" }.enter();
//
//	sim_assert(!valid(rx) | rx->header(TlpOffset::dw0.type) == 0) << "not a memory tlp";
//
//	// decode command
//	avmm.address = (UInt) rx->header(64, 32_b);
//	avmm.address(0, 2_b) = 0;
//
//	avmm.write = transfer(rx) & isDataTlp(rx->header);
//	avmm.read = transfer(rx) & !*avmm.write;
//	avmm.writeData = (UInt) rx->data;
//
//	// store cpl data for command
//	size_t pipelineDepth = std::max<size_t>(avmm.maximumPendingReadTransactions, 1);
//	Fifo<MemTlpCplData> resQueue{ pipelineDepth , MemTlpCplData{} };
//
//	MemTlpCplData req;
//	req.decode(rx->header);
//	HCL_NAMED(req);
//	IF(*avmm.read)
//		resQueue.push(req);
//
//	// create tx tlp
//	avmm.createReadDataValid();
//	valid(m_tx) = *avmm.readDataValid;
//	m_tx->header = "96x00000000000000044a000001";
//	m_tx->header(48, 16_b) = (BVec) pack(m_cplId);
//	m_tx->data = (BVec) *avmm.readData;
//
//	// join response and cpl data
//	MemTlpCplData res = resQueue.peek();
//	IF(valid(m_tx))
//		resQueue.pop();
//	res.encode(m_tx->header);
//
//	ready(rx) = !resQueue.full();
//	if (avmm.ready)
//		ready(rx) &= *avmm.ready;
//
//	resQueue.generate();
//}
//
//gtry::scl::pci::Tlp gtry::scl::pci::Tlp::discardHighAddressBits() const
//{
//	Tlp tlpHdr{
//		.prefix = prefix,
//		.header = header(0, 96_b),
//		.data = data
//	};
//
//	if (header.width() > 96_b)
//		IF(header[TlpOffset::has4DW])
//		tlpHdr.header(64, 32_b) = header(96, 32_b);
//
//	HCL_NAMED(tlpHdr);
//	return tlpHdr;
//}
//
//void gtry::scl::pci::MemTlpCplData::decode(const BVec& tlpHdr)
//{
//	attr.trafficClass = tlpHdr(20, 3_b);
//	attr.idBasedOrdering = tlpHdr[18];
//	attr.relaxedOrdering = tlpHdr[13];
//	attr.noSnoop = tlpHdr[12];
//	lowerAddress = cat(tlpHdr(66, 5_b), "b00");
//
//	tag = tlpHdr(40, 8_b);
//	requester.func = tlpHdr(48, 3_b);
//	requester.dev = tlpHdr(48 + 3, 5_b);
//	requester.bus = tlpHdr(56, 8_b);
//}
//
//void gtry::scl::pci::MemTlpCplData::encode(BVec& tlpHdr) const
//{
//	tlpHdr(20, 3_b) = attr.trafficClass;
//	tlpHdr[18] = attr.idBasedOrdering;
//	tlpHdr[13] = attr.relaxedOrdering;
//	tlpHdr[12] = attr.noSnoop;
//
//	tlpHdr(64, 32_b) = (BVec) cat(requester.bus, requester.dev, requester.func, tag, '0', lowerAddress);
//}

//void gtry::scl::pci::IntelPTileCompleter::generate()
//{
//	auto entity = Area{ "IntelPTileCompleter" }.enter();
//
//	static_assert(Signal<BVec&>);
//	static_assert(Signal<Tlp&>);
//	static_assert(Signal<RvPacketStream<Tlp>&>);
//	static_assert(CompoundSignal<RvPacketStream<Tlp>>);
//	static_assert(Signal<std::array<RvPacketStream<Tlp>, 2>&>);
//
//	Bit anyValid = '0';
//	std::array<VPacketStream<Tlp>, 2> in_reduced;
//	for (size_t i = 0; i < in.size(); ++i)
//	{
//		in[i]->header = 128_b;
//		in[i]->data = 32_b;
//
//		BVec header = swapEndian(in[i]->header, 8_b, 32_b);
//		IF(header[29]) // 4 DW tlp header
//			header(64, 32_b) = header(96, 32_b);
//
//		valid(in_reduced[i]) = valid(in[i]) & sop(in[i]) & eop(in[i]);
//		*in_reduced[i] = *in[i];
//		in_reduced[i]->header = header(0, 96_b);
//		anyValid |= valid(in_reduced[i]);
//	}
//
//	Fifo in_buffer(32, in_reduced);
//	IF(anyValid)
//		in_buffer.push(in_reduced);
//
//	ready(in[0])= in_buffer.almostEmpty(32 - 27);
//	for (size_t i = 1; i < in.size(); ++i)
//		ready(in[i]) = ready(in[0]);
//
//	std::array<RvStream<Tlp>, 2> in_buffered{
//		RvStream<Tlp>{
//			*in_buffer.peek()[0],
//			{
//				Ready{},
//				Valid{valid(in_buffer.peek()[0])},
//			}
//		},
//		RvStream<Tlp>{
//			*in_buffer.peek()[1],
//			{
//				Ready{},
//				Valid{valid(in_buffer.peek()[1])},
//			}
//		},
//	};
//
//	arbitrateInOrder in_single{ in_buffered[0], in_buffered[1] };
//	IF(transfer(in_buffered[0]) | transfer(in_buffered[1]))
//		in_buffer.pop();
//
//	AvalonMM avmm;
//	avmm.ready = Bit{};
//	avmm.read = Bit{};
//	avmm.write = Bit{};
//	avmm.writeData = 32_b;
//	avmm.readData = 32_b;
//	avmm.readDataValid = Bit{};
//
//	AvmmBridge bridge{ in_single, avmm, completerId };
//
//}

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


	//pci::TlpCommand pci::Header::decode(pci::Support support)
	//{
	//	pci::TlpCommand ret;
	//	if(support.addressing32bit) IF(dw0.fmt == 0b000 & dw0.type == 0b00000) ret = pci::TlpCommand::memoryReadRequest32bit;
	//	if(support.addressing64bit) IF(dw0.fmt == 0b001 & dw0.type == 0b00000) ret = pci::TlpCommand::memoryReadRequest64bit;
	//
	//	if(support.lockedOPs)
	//	{
	//		if(support.addressing32bit) IF(dw0.fmt == 0b000 & dw0.type == 0b00001) ret = pci::TlpCommand::memoryReadRequestLocked32bit;
	//		if(support.addressing64bit) IF(dw0.fmt == 0b001 & dw0.type == 0b00000) ret = pci::TlpCommand::memoryReadRequestLocked64bit;
	//	}
	//
	//	if(support.addressing32bit) IF(dw0.fmt == 0b010 & dw0.type == 0b00000) ret = pci::TlpCommand::memoryWriteRequest32bit;
	//	if(support.addressing64bit) IF(dw0.fmt == 0b011 & dw0.type == 0b00000) ret = pci::TlpCommand::memoryWriteRequest64bit;
	//
	//	if(support.ioOps) {
	//		IF(dw0.fmt == 0b000 & dw0.type == 0b00010) ret = pci::TlpCommand::ioReadRequest;
	//		IF(dw0.fmt == 0b010 & dw0.type == 0b00010) ret = pci::TlpCommand::ioWriteRequest;
	//	}
	//	if(support.configOps) {
	//		IF(dw0.fmt == 0b000 & dw0.type == 0b00100) ret = pci::TlpCommand::configurationReadType0;
	//		IF(dw0.fmt == 0b010 & dw0.type == 0b00100) ret = pci::TlpCommand::configurationWriteType0;
	//		IF(dw0.fmt == 0b000 & dw0.type == 0b00101) ret = pci::TlpCommand::configurationReadType1;
	//		IF(dw0.fmt == 0b010 & dw0.type == 0b00101) ret = pci::TlpCommand::configurationWriteType1;
	//	}
	//	if(support.messageOps) {
	//		IF(dw0.fmt == 0b001 & dw0.type.upper(2_b) == 0b10) ret = pci::TlpCommand::messageRequest;
	//		IF(dw0.fmt == 0b011 & dw0.type.upper(2_b) == 0b10) ret = pci::TlpCommand::messageRequestWithDataPayload;
	//	}
	//
	//	IF(dw0.fmt == 0b000 & dw0.type == 0b01010) ret = pci::TlpCommand::completionWithoutData;
	//	IF(dw0.fmt == 0b010 & dw0.type == 0b01010) ret = pci::TlpCommand::completionWithData;
	//
	//	if(support.lockedOPs)
	//	{
	//		IF(dw0.fmt == 0b000 & dw0.type == 0b01011) ret = pci::TlpCommand::completionforLockedMemoryReadWithoutData;
	//		IF(dw0.fmt == 0b010 & dw0.type == 0b01011) ret = pci::TlpCommand::completionforLockedMemoryReadWithData;
	//	}
	//
	//	if(support.atomicOps) {
	//		if(support.addressing32bit) IF(dw0.fmt == 0b010 & dw0.type == 0b01100) ret = pci::TlpCommand::fetchAndAddAtomicOpRequest32bit;
	//		if(support.addressing64bit) IF(dw0.fmt == 0b011 & dw0.type == 0b01100) ret = pci::TlpCommand::fetchAndAddAtomicOpRequest64bit;
	//		if(support.addressing32bit) IF(dw0.fmt == 0b010 & dw0.type == 0b01101) ret = pci::TlpCommand::unconditionalSwapAtomicOpRequest32bit;
	//		if(support.addressing64bit) IF(dw0.fmt == 0b011 & dw0.type == 0b01101) ret = pci::TlpCommand::unconditionalSwapAtomicOpRequest64bit;
	//		if(support.addressing32bit) IF(dw0.fmt == 0b010 & dw0.type == 0b01110) ret = pci::TlpCommand::compareAndSwapAtomicOpRequest32bit;
	//		if(support.addressing64bit) IF(dw0.fmt == 0b011 & dw0.type == 0b01110) ret = pci::TlpCommand::compareAndSwapAtomicOpRequest64bit;
	//	}
	//	return ret;
	//}
	//
	//void pci::Header::encode(TlpCommand command,  Support support)
	//{
	//	if(support.addressing32bit) IF(command == pci::TlpCommand::memoryReadRequest32bit) dw0.fmt = 0b000 ; dw0.type = 0b00000 ;
	//	if(support.addressing64bit) IF(command == pci::TlpCommand::memoryReadRequest64bit) dw0.fmt = 0b001 ; dw0.type = 0b00000 ;
	//
	//	if (support.lockedOPs)
	//	{
	//		if(support.addressing32bit) IF(command == pci::TlpCommand::memoryReadRequestLocked32bit) dw0.fmt = 0b000 ; dw0.type = 0b00001 ;
	//		if(support.addressing64bit) IF(command == pci::TlpCommand::memoryReadRequestLocked64bit) dw0.fmt = 0b001 ; dw0.type = 0b00000 ;
	//	}
	//
	//	if(support.addressing32bit) IF(command == pci::TlpCommand::memoryWriteRequest32bit) dw0.fmt = 0b010 ; dw0.type = 0b00000 ;
	//	if(support.addressing64bit) IF(command == pci::TlpCommand::memoryWriteRequest64bit) dw0.fmt = 0b011 ; dw0.type = 0b00000 ;
	//
	//	if(support.ioOps) {
	//		IF(command == pci::TlpCommand::ioReadRequest) dw0.fmt = 0b000 ; dw0.type = 0b00010 ;
	//		IF(command == pci::TlpCommand::ioWriteRequest) dw0.fmt = 0b010 ; dw0.type = 0b00010 ;
	//	}
	//	if(support.configOps) {
	//		IF(command == pci::TlpCommand::configurationReadType0) dw0.fmt = 0b000 ; dw0.type = 0b00100 ;
	//		IF(command == pci::TlpCommand::configurationWriteType0) dw0.fmt = 0b010 ; dw0.type = 0b00100 ;
	//		IF(command == pci::TlpCommand::configurationReadType1) dw0.fmt = 0b000 ; dw0.type = 0b00101 ;
	//		IF(command == pci::TlpCommand::configurationWriteType1) dw0.fmt = 0b010 ; dw0.type = 0b00101 ;
	//	}
	//	if(support.messageOps) {
	//		IF(command == pci::TlpCommand::messageRequest) dw0.fmt = 0b001; dw0.type.upper(2_b) = 0b10;
	//		IF(command == pci::TlpCommand::messageRequestWithDataPayload) dw0.fmt = 0b011; dw0.type.upper(2_b) = 0b10;
	//	}
	//
	//	IF(command == pci::TlpCommand::completionWithoutData) dw0.fmt = 0b000 ; dw0.type = 0b01010 ;
	//	IF(command == pci::TlpCommand::completionWithData) dw0.fmt = 0b010 ; dw0.type = 0b01010 ;
	//
	//	if (support.lockedOPs)
	//	{
	//		IF(command == pci::TlpCommand::completionforLockedMemoryReadWithoutData) dw0.fmt = 0b000 ; dw0.type = 0b01011 ;
	//		IF(command == pci::TlpCommand::completionforLockedMemoryReadWithData) dw0.fmt = 0b010 ; dw0.type = 0b01011 ;
	//	}
	//
	//	if (support.atomicOps) {
	//		if(support.addressing32bit) IF(command == pci::TlpCommand::fetchAndAddAtomicOpRequest32bit) dw0.fmt = 0b010 ; dw0.type = 0b01100 ;
	//		if(support.addressing64bit) IF(command == pci::TlpCommand::fetchAndAddAtomicOpRequest64bit) dw0.fmt = 0b011 ; dw0.type = 0b01100 ;
	//		if(support.addressing32bit) IF(command == pci::TlpCommand::unconditionalSwapAtomicOpRequest32bit) dw0.fmt = 0b010 ; dw0.type = 0b01101 ;
	//		if(support.addressing64bit) IF(command == pci::TlpCommand::unconditionalSwapAtomicOpRequest64bit) dw0.fmt = 0b011 ; dw0.type = 0b01101 ;
	//		if(support.addressing32bit) IF(command == pci::TlpCommand::compareAndSwapAtomicOpRequest32bit) dw0.fmt = 0b010 ; dw0.type = 0b01110 ;
	//		if(support.addressing64bit) IF(command == pci::TlpCommand::compareAndSwapAtomicOpRequest64bit) dw0.fmt = 0b011 ; dw0.type = 0b01110 ;
	//	}
	//
	//}

}

