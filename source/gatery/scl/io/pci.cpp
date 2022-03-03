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
#include "../StreamArbiter.h"

gtry::Bit gtry::scl::pci::isCompletionTlp(const BVec& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.getWidth() >= 8_b, "first 8b of tlp header required for decoding");
	Bit completion_tlp = tlpHeader(0, 5) == "b01010";
	HCL_NAMED(completion_tlp);
	return completion_tlp;
}

gtry::Bit gtry::scl::pci::isMemTlp(const BVec& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.getWidth() >= 8_b, "first 8b of tlp header required for decoding");
	Bit mem_tlp = tlpHeader(0, 5) == 0;
	HCL_NAMED(mem_tlp);
	return mem_tlp;
}

gtry::Bit gtry::scl::pci::isDataTlp(const BVec& tlpHeader)
{
	HCL_DESIGNCHECK_HINT(tlpHeader.getWidth() >= 32_b, "first word of tlp header required for decoding");
	Bit data_tlp = tlpHeader[30];
	HCL_NAMED(data_tlp);
	return data_tlp;
}

gtry::scl::pci::AvmmBridge::AvmmBridge(Stream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId) :
	AvmmBridge()
{
	setup(rx, avmm, cplId);
}

void gtry::scl::pci::AvmmBridge::setup(Stream<Tlp>& rx, AvalonMM& avmm, const PciId& cplId)
{
	HCL_DESIGNCHECK_HINT(rx.data.header.getWidth() == 96_b, "reduce tlp header address using discardHighAddressBits");
	m_cplId = cplId;
	generateFifoBridge(rx, avmm);
}

void gtry::scl::pci::AvmmBridge::generateFifoBridge(Stream<Tlp>& rx, AvalonMM& avmm)
{
	HCL_DESIGNCHECK(avmm.readData->getWidth() == 32_b);

	auto entity = Area{ "AvmmBridge" }.enter();

	sim_assert(!*rx.valid | rx.data.header(TlpOffset::type) == 0) << "not a memory tlp";

	// decode command
	avmm.address = rx.data.header(64, 32_b);
	avmm.address(0, 2_b) = 0;

	avmm.write = rx.transfer() & isDataTlp(rx.data.header);
	avmm.read = rx.transfer() & !*avmm.write;
	avmm.writeData = rx.data.data;

	// store cpl data for command
	size_t pipelineDepth = std::max<size_t>(avmm.maximumPendingReadTransactions, 1);
	Fifo<MemTlpCplData> resQueue{ pipelineDepth , MemTlpCplData{} };
	
	MemTlpCplData req;
	req.decode(rx.data.header);
	HCL_NAMED(req);
	resQueue.push(req, *avmm.read);

	// create tx tlp
	avmm.createReadDataValid();
	m_tx.valid = *avmm.readDataValid;
	m_tx.data.header = "96x00000000000000044a000001";
	m_tx.data.header(48, 16_b) = pack(m_cplId);
	m_tx.data.data = *avmm.readData;

	// join response and cpl data
	MemTlpCplData res;
	resQueue.pop(res, *m_tx.valid);
	res.encode(m_tx.data.header);

	*rx.ready = !resQueue.full();
	if (avmm.ready)
		*rx.ready &= *avmm.ready;

}

gtry::scl::pci::Tlp gtry::scl::pci::Tlp::discardHighAddressBits() const
{
	Tlp tlpHdr{
		.prefix = prefix,
		.header = header(0, 96_b),
		.data = data
	};

	if(header.getWidth() > 96_b)
		IF(header[TlpOffset::has4DW])
			tlpHdr.header(64, 32_b) = header(96, 32_b);

	HCL_NAMED(tlpHdr);
	return tlpHdr;
}

void gtry::scl::pci::MemTlpCplData::decode(const BVec& tlpHdr)
{
	attr.trafficClass = tlpHdr(20, 3_b);
	attr.idBasedOrdering = tlpHdr[18];
	attr.relaxedOrdering = tlpHdr[13];
	attr.noSnoop = tlpHdr[12];
	lowerAddress = pack(tlpHdr(66, 5_b), "b00");

	tag = tlpHdr(40, 8_b);
	requester.func = tlpHdr(48, 3_b);
	requester.dev = tlpHdr(48 + 3, 5_b);
	requester.bus = tlpHdr(56, 8_b);
}

void gtry::scl::pci::MemTlpCplData::encode(BVec& tlpHdr) const
{
	tlpHdr(20, 3_b) = attr.trafficClass;
	tlpHdr[18] = attr.idBasedOrdering;
	tlpHdr[13] = attr.relaxedOrdering;
	tlpHdr[12] = attr.noSnoop;

	tlpHdr(64, 32_b) = pack(requester.bus, requester.dev, requester.func, tag, '0', lowerAddress);
}

void gtry::scl::pci::IntelPTileCompleter::generate()
{
	auto entity = Area{ "IntelPTileCompleter" }.enter();

	static_assert(Signal<BVec&>);
	static_assert(Signal<Tlp&>);
	static_assert(Signal<Stream<Tlp>&>);
	static_assert(CompoundSiganl<Stream<Tlp>>);
	static_assert(Signal<std::array<Stream<Tlp>, 2>&>);



	Bit anyValid = '0';
	std::array<Stream<Tlp>, 2> in_reduced;
	for (size_t i = 0; i < in.size(); ++i)
	{
		in[i].ready.emplace();
		in[i].valid.emplace();
		in[i].sop.emplace();
		in[i].eop.emplace();
		in[i].data.header = 128_b;
		in[i].data.data = 32_b;

		BVec header = swapEndian(in[i].data.header, 8_b, 32_b);
		IF(header[29]) // 4 DW tlp header
			header(64, 32_b) = header(96, 32_b);

		in_reduced[i].valid = *in[i].valid & *in[i].sop & *in[i].eop;
		in_reduced[i].data = in[i].data;
		in_reduced[i].data.header = header(0, 96_b);
		anyValid |= *in_reduced[i].valid;
	}

	Fifo in_buffer(32, in_reduced);
	in_buffer.push(in_reduced, anyValid);

	*in[0].ready = in_buffer.almostEmpty(32 - 27);
	for (size_t i = 1; i < in.size(); ++i)
		*in[i].ready = *in[0].ready;

	
	std::array<Stream<Tlp>, 2> in_buffered;
	Bit in_buffer_pop;
	in_buffer.pop(in_buffered, in_buffer_pop);

	arbitrateInOrder in_single{ in_buffered[0], in_buffered[1] };
	in_buffer_pop = in_buffered[0].transfer() | in_buffered[1].transfer();

	AvalonMM avmm;
	avmm.ready = Bit{};
	avmm.read = Bit{};
	avmm.write = Bit{};
	avmm.writeData = 32_b;
	avmm.readData = 32_b;
	avmm.readDataValid = Bit{};

	AvmmBridge bridge{ in_single, avmm, completerId };

	Fifo<Stream<Tlp>> out_buffer{ 16, bridge.tx() };
	out_buffer.push(bridge.tx(), bridge.tx().transfer());


	


}
