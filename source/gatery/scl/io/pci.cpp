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

gtry::Bit gtry::scl::pci::isCompletionTlp(const UInt& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 8_b, "first 8b of tlp header required for decoding");
	Bit completion_tlp = tlpHeader(0, 5) == "b01010";
	HCL_NAMED(completion_tlp);
	return completion_tlp;
}

gtry::Bit gtry::scl::pci::isMemTlp(const UInt& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 8_b, "first 8b of tlp header required for decoding");
	Bit mem_tlp = tlpHeader(0, 5) == 0;
	HCL_NAMED(mem_tlp);
	return mem_tlp;
}

gtry::Bit gtry::scl::pci::isDataTlp(const UInt& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.width() >= 32_b, "first word of tlp header required for decoding");
	Bit data_tlp = tlpHeader[30];
	HCL_NAMED(data_tlp);
	return data_tlp;
}

gtry::scl::pci::AvmmBridge::AvmmBridge(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId) :
	AvmmBridge()
{
	setup(rx, avmm, cplId);
}

void gtry::scl::pci::AvmmBridge::setup(RvStream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId)
{
	HCL_DESIGNCHECK_HINT(rx->header.width() == 96_b, "reduce tlp header address using discardHighAddressBits");
	m_cplId = cplId;
	generateFifoBridge(rx, avmm);
}

void gtry::scl::pci::AvmmBridge::generateFifoBridge(RvStream<Tlp>& rx, AvalonMM& avmm)
{
	HCL_DESIGNCHECK(avmm.readData->width() == 32_b);

	auto entity = Area{ "AvmmBridge" }.enter();

	sim_assert(!valid(rx) | rx->header(TlpOffset::type) == 0) << "not a memory tlp";

	// decode command
	avmm.address = rx->header(64, 32_b);
	avmm.address(0, 2_b) = 0;

	avmm.write = transfer(rx) & isDataTlp(rx->header);
	avmm.read = transfer(rx) & !*avmm.write;
	avmm.writeData = rx->data;

	// store cpl data for command
	size_t pipelineDepth = std::max<size_t>(avmm.maximumPendingReadTransactions, 1);
	Fifo<MemTlpCplData> resQueue{ pipelineDepth , MemTlpCplData{} };

	MemTlpCplData req;
	req.decode(rx->header);
	HCL_NAMED(req);
	IF(*avmm.read)
		resQueue.push(req);

	// create tx tlp
	avmm.createReadDataValid();
	valid(m_tx) = *avmm.readDataValid;
	m_tx->header = "96x00000000000000044a000001";
	m_tx->header(48, 16_b) = pack(m_cplId);
	m_tx->data = *avmm.readData;

	// join response and cpl data
	MemTlpCplData res = resQueue.peak();
	IF(valid(m_tx))
		resQueue.pop();
	res.encode(m_tx->header);

	ready(rx) = !resQueue.full();
	if (avmm.ready)
		ready(rx) &= *avmm.ready;

	resQueue.generate();
}

gtry::scl::pci::Tlp gtry::scl::pci::Tlp::discardHighAddressBits() const
{
	Tlp tlpHdr{
		.prefix = prefix,
		.header = header(0, 96_b),
		.data = data
	};

	if (header.width() > 96_b)
		IF(header[TlpOffset::has4DW])
		tlpHdr.header(64, 32_b) = header(96, 32_b);

	HCL_NAMED(tlpHdr);
	return tlpHdr;
}

void gtry::scl::pci::MemTlpCplData::decode(const UInt& tlpHdr)
{
	attr.trafficClass = tlpHdr(20, 3_b);
	attr.idBasedOrdering = tlpHdr[18];
	attr.relaxedOrdering = tlpHdr[13];
	attr.noSnoop = tlpHdr[12];
	lowerAddress = cat(tlpHdr(66, 5_b), "b00");

	tag = tlpHdr(40, 8_b);
	requester.func = tlpHdr(48, 3_b);
	requester.dev = tlpHdr(48 + 3, 5_b);
	requester.bus = tlpHdr(56, 8_b);
}

void gtry::scl::pci::MemTlpCplData::encode(UInt& tlpHdr) const
{
	tlpHdr(20, 3_b) = attr.trafficClass;
	tlpHdr[18] = attr.idBasedOrdering;
	tlpHdr[13] = attr.relaxedOrdering;
	tlpHdr[12] = attr.noSnoop;

	tlpHdr(64, 32_b) = cat(requester.bus, requester.dev, requester.func, tag, '0', lowerAddress);
}

void gtry::scl::pci::IntelPTileCompleter::generate()
{
	auto entity = Area{ "IntelPTileCompleter" }.enter();

	static_assert(Signal<BVec&>);
	static_assert(Signal<Tlp&>);
	static_assert(Signal<RvPacketStream<Tlp>&>);
	static_assert(CompoundSignal<RvPacketStream<Tlp>>);
	static_assert(Signal<std::array<RvPacketStream<Tlp>, 2>&>);

	Bit anyValid = '0';
	std::array<VPacketStream<Tlp>, 2> in_reduced;
	for (size_t i = 0; i < in.size(); ++i)
	{
		in[i]->header = 128_b;
		in[i]->data = 32_b;

		UInt header = swapEndian(in[i]->header, 8_b, 32_b);
		IF(header[29]) // 4 DW tlp header
			header(64, 32_b) = header(96, 32_b);

		valid(in_reduced[i]) = valid(in[i]) & sop(in[i]) & eop(in[i]);
		*in_reduced[i] = *in[i];
		in_reduced[i]->header = header(0, 96_b);
		anyValid |= valid(in_reduced[i]);
	}

	Fifo in_buffer(32, in_reduced);
	IF(anyValid)
		in_buffer.push(in_reduced);

	ready(in[0])= in_buffer.almostEmpty(32 - 27);
	for (size_t i = 1; i < in.size(); ++i)
		ready(in[i]) = ready(in[0]);

	std::array<RvStream<Tlp>, 2> in_buffered{
		RvStream<Tlp>{
			*in_buffer.peak()[0],
			{
				Ready{},
				Valid{valid(in_buffer.peak()[0])},
			}
		},
		RvStream<Tlp>{
			*in_buffer.peak()[1],
			{
				Ready{},
				Valid{valid(in_buffer.peak()[1])},
			}
		},
	};

	arbitrateInOrder in_single{ in_buffered[0], in_buffered[1] };
	IF(transfer(in_buffered[0]) | transfer(in_buffered[1]))
		in_buffer.pop();

	AvalonMM avmm;
	avmm.ready = Bit{};
	avmm.read = Bit{};
	avmm.write = Bit{};
	avmm.writeData = 32_b;
	avmm.readData = 32_b;
	avmm.readDataValid = Bit{};

	AvmmBridge bridge{ in_single, avmm, completerId };

}
