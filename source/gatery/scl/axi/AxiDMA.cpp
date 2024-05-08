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
#include "AxiDMA.h"

#include "../stream/streamFifo.h"

namespace gtry::scl
{
	RvStream<AxiAddress> axiGenerateAddressFromCommand(RvStream<AxiToStreamCmd>&& cmdIn, const AxiConfig& config)
	{
		Area ent{ "scl_axiGenerateAddressFromCommand", true };

		// improve timing by first calculating the last address
		struct LastAddress { UInt addr; };
		scl::Stream cmd = cmdIn.add(LastAddress{ cmdIn->endAddress - cmdIn->bytesPerBurst })
			| scl::strm::regDownstream();

		RvStream<AxiAddress> out;
		out->addr = config.addrW;
		out->id = zext(cmd->id, config.idW);
		out->user = ConstBVec(config.arUserW);

		HCL_DESIGNCHECK_HINT(cmd->bytesPerBurst >= config.alignedDataW().bytes(), "Burst size must be at least as large as the data width of the AXI interface.");
		out->size = utils::Log2C(config.dataW.bytes());

		HCL_DESIGNCHECK_HINT(cmd->bytesPerBurst % config.alignedDataW().bytes() == 0, "Burst size must be a multiple of the data width of the AXI interface.");
		out->len = cmd->bytesPerBurst / config.alignedDataW().bytes() - 1;

		out->burst = (size_t)AxiBurstType::INCR;
		out->cache = 0;
		out->prot = 0;
		out->qos = 0;
		out->region = 0;

		valid(out) = valid(cmd);

		// address calculation logic. 
		// first out beat is in IDLE state with cmd->startAddress followed by incremental address register beats.
		enum State { IDLE, TRANSFER };
		Reg<Enum<State>> state{ IDLE };
		state.setName("state");

		UInt address = cmd->endAddress.width();
		HCL_NAMED(address);

		IF(state.current() == IDLE)
		{
			address = cmd->startAddress;
			IF(transfer(out))
				state = TRANSFER;
		}

		ready(cmd) = '0';
		IF(state.combinatorial() == TRANSFER)
		{
			IF(transfer(out) & address == cmd.template get<LastAddress>().addr)
			{
				ready(cmd) = '1';
				state = IDLE;
			}
		}

		out->addr = zext(address);
		IF(transfer(out))
			address += cmd->bytesPerBurst;

		address = reg(address);

		HCL_NAMED(out);
		return out;
	}

	RvPacketStream<BVec> axiToStream(RvPacketStream<AxiReadData>&& dataStream)
	{
		return dataStream.transform(&AxiReadData::data);
	}

	RvPacketStream<BVec> axiToStream(RvStream<AxiToStreamCmd>&& cmd, Axi4& axi)
	{
		Area area{ "scl_axiToStream", true };
		HCL_NAMED(axi);
		HCL_NAMED(cmd);

		*axi.ar <<= axiGenerateAddressFromCommand(move(cmd), axi.config());
		Stream out = axiToStream(move(axi.r));
		HCL_NAMED(out);
		return out;
	}

	void axiFromStream(RvStream<BVec>&& in, RvPacketStream<AxiWriteData>& out, size_t beatsPerBurst)
	{
		scl::Counter beatCtr{ beatsPerBurst };
		HCL_DESIGNCHECK_HINT(in->width() == out->data.width(), "the data stream and axi data widths do not match");
		out <<= in.transform([&](const BVec& data) {
			return AxiWriteData{
				.data = data,
				.strb = BVec{out->strb.width().mask()},
				.user = ConstBVec(out->user.width()),
			};
		}).add(Eop{ beatCtr.isLast() });

		IF(transfer(out))
			beatCtr.inc();
	}

	void axiFromStream(RvStream<AxiToStreamCmd>&& cmd, RvStream<BVec>&& data, Axi4& axi)
	{
		Area area{ "scl_axiFromStream", true };
		HCL_NAMED(data);
		HCL_NAMED(cmd);
		

		*axi.aw <<= axiGenerateAddressFromCommand(move(cmd), axi.config());
		axiFromStream(move(data), *axi.w, cmd->bytesPerBurst / axi.config().alignedDataW().bytes());
		ready(axi.b) = '1';
		HCL_NAMED(axi);
	}

	void axiDMA(RvStream<AxiToStreamCmd>&& fetchCmd, RvStream<AxiToStreamCmd>&& storeCmd, Axi4& axi, size_t dataFifoDepth)
	{
		Area area{ "scl_axiDma", true };

		scl::Stream mid = scl::axiToStream(move(fetchCmd), axi);
		HCL_NAMED(mid);

		if (dataFifoDepth > 1)
			mid = strm::fifo(move(mid), dataFifoDepth, FifoLatency(0));
		if (dataFifoDepth > 0)
			mid = strm::regDownstream(move(mid));

		scl::axiFromStream(move(storeCmd), mid.template remove<scl::Eop>(), axi);
	}

	AxiTransferReport axiTransferAuditor(const Axi4& streamToSniff, BitWidth bitsPerBurst, BitWidth counterW)
	{
		Area area{ "scl_axiTransferAuditor", true };
		Counter burstCounter(counterW.last());
		IF(transfer(streamToSniff.b))
			burstCounter.inc();

		Counter failCounter(counterW.last());
		IF(transfer(streamToSniff.b))
			IF(streamToSniff.b->resp == (size_t) AxiResponseCode::SLVERR | streamToSniff.b->resp == (size_t) AxiResponseCode::DECERR)
				failCounter.inc();

		return AxiTransferReport{.burstCount = burstCounter.value(), .failCount = failCounter.value(), .bitsPerBurst = bitsPerBurst.bits()};
	}
}
