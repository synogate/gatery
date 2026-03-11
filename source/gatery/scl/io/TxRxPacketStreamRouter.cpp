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
#include "TxRxPacketStreamRouter.h"
#include "../stream/StreamArbiter.h"

namespace gtry::scl 
{
	TxRxPacketStreamRouter& TxRxPacketStreamRouter::attach(size_t routingId, TxRxPacketStream&& stream)
	{
		if(!m_sinks.empty())
		{
			const TxRxPacketStream& o = m_sinks.cbegin()->second;
			HCL_DESIGNCHECK(stream.tx->width() == o.tx->width());
			HCL_DESIGNCHECK((*stream.rx)->width() == (*o.rx)->width());
		}
		m_sinks.erase(routingId);
		m_sinks.emplace(routingId, move(stream));
		return *this;
	}

	TxRxPacketStreamRouter& TxRxPacketStreamRouter::attach(size_t routingId, TxRxStream&& stream)
	{
		TxRxPacketStream pks{ {stream.tx->width()}, {(*stream.rx)->width()} };
		pks.tx <<= move(stream.tx) | strm::attach(Eop{ '1' });
		*stream.rx <<= move(*pks.rx) | strm::remove<Eop>();
		return attach(routingId, move(pks));
	}

	TxRxPacketStream TxRxPacketStreamRouter::generate()
	{
		Area ent{ "scl_TxRxPacketStreamRouter", true };

		BitWidth dataW = m_sinks.empty() ? 0_b : m_sinks.cbegin()->second.tx->width();
		TxRxPacketStream out = { {dataW}, {dataW} };

		// tx
		StreamArbiter<RvPacketStream<BVec>> txArbiter;
		for (auto& sink : m_sinks)
			move(sink.second.tx) |
				strm::insertBeat(0, zext(BVec{ sink.first })) |
				txArbiter();
		out.tx <<= txArbiter.generate().out();

		// rx
		UInt rxRoutingId = (UInt)reg(**out.rx, { .en = sop(*out.rx) }) | name("rxRoutingId");
		StreamDemux<RvPacketStream<BVec>> rxDemux(move(*out.rx) | strm::eraseBeat(0, 1), rxRoutingId);
		for (auto& it : m_sinks)
			*it.second.rx <<= rxDemux.out(it.first);

		return out;
	}

	TxRxStream replaceEopByLengthHeader(TxRxPacketStream&& in, size_t maxPacketBeats)
	{
		Area ent{ "scl_replaceEopByLengthHeader", true };
		TxRxStream out = { {in.tx->width()}, {(*in.rx)->width()} };

		// tx
		out.tx <<= move(in.tx) | strm::prependPacketLengthHeader(maxPacketBeats) | strm::remove<Eop>();
		
		// rx
		*in.rx <<= move(*out.rx) | strm::attachEopFromPacketLengthHeader(maxPacketBeats);

		HCL_NAMED(out);
		return out;
	}
}
