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
#include "tilelink.h"

namespace gtry::scl
{
	template struct Stream<TileLinkA, Ready, Valid>;
	template struct Stream<TileLinkD, Ready, Valid>;

	template UInt transferLength(const TileLinkChannelA&);
	template UInt transferLength(const TileLinkChannelD&);
	template std::tuple<Sop, Eop> seop(const TileLinkChannelA&);
	template std::tuple<Sop, Eop> seop(const TileLinkChannelD&);
	template Bit sop(const TileLinkChannelA&);
	template Bit sop(const TileLinkChannelD&);
	template Bit eop(const TileLinkChannelA&);
	template Bit eop(const TileLinkChannelD&);

	void setFullByteEnableMask(TileLinkChannelA& a)
	{
		a->mask = (BVec)sext(1);

		const UInt& size = a->size;
		const UInt& offset = a->address(0, BitWidth::count(a->mask.width().bits()));
		for (size_t i = 0; (1ull << i) < a->mask.width().bits(); i++)
		{
			IF(size == i)
			{
				a->mask = (BVec)zext(0);
				a->mask(offset, BitWidth{ 1ull << i }) = (BVec)sext(1);
			}
		}
	}

	UInt transferLengthFromLogSize(const UInt& logSize, size_t numSymbolsPerBeat)
	{
		BitWidth beatWidth = BitWidth::count(numSymbolsPerBeat);
		UInt size = decoder(logSize);
		UInt beats = size.upper(size.width() - beatWidth);
		beats.lsb() |= size.lower(beatWidth) != 0;
		return beats;
	}

	TileLinkD tileLinkDefaultResponse(const TileLinkA& request)
	{
		TileLinkD res;
		res.opcode = (size_t)TileLinkD::AccessAck;
		IF(request.opcode == (size_t)TileLinkA::Get)
			res.opcode = (size_t)TileLinkD::AccessAckData;

		res.param = 0;
		res.size = request.size;
		res.source = request.source;
		res.sink = 0u;
		res.data = ConstBVec(request.data.width());
		res.error = '0';

		return res;
	}

	void connect(Memory<BVec>& mem, TileLinkUL& link)
	{
		BitWidth byteOffsetW = BitWidth::count(link.a->mask.width().bits());
		HCL_DESIGNCHECK(mem.wordSize() == link.a->data.width());
		HCL_DESIGNCHECK(mem.addressWidth() >= link.a->address.width() - byteOffsetW);

		TileLinkChannelD d;
		*d = tileLinkDefaultResponse(*link.a);
		valid(d) = valid(link.a);

		auto port = mem[link.a->address.upper(-byteOffsetW)];
		d->data = port.read();

		IF(link.a->opcode == (size_t)TileLinkA::PutFullData |
			link.a->opcode == (size_t)TileLinkA::PutPartialData)
		{
			BVec writeData = d->data;
			for (size_t i = 0; i < link.a->mask.size(); ++i)
				IF(link.a->mask[i])
					writeData(i * 8, 8_b) = link.a->data(i * 8, 8_b);

			IF(transfer(link.a))
				port = writeData;

			d->data = ConstBVec(mem.wordSize());
		}

		// create downstream registers
		valid(d).resetValue('0');
		for (size_t i = 0; i < mem.readLatencyHint(); ++i)
			d = d.regDownstreamBlocking({ .allowRetimingBackward = true });

		ready(link.a) = ready(d);
		*link.d <<= d.regReady();
	}
}

namespace gtry 
{
	template class Reverse<scl::TileLinkChannelD>;

	template void connect(scl::TileLinkUL&, scl::TileLinkUL&);
	template void connect(scl::TileLinkUH&, scl::TileLinkUH&);
	template void connect(scl::TileLinkChannelA&, scl::TileLinkChannelA&);
	template void connect(scl::TileLinkChannelD&, scl::TileLinkChannelD&);

	template auto upstream(scl::TileLinkUL&);
	template auto upstream(scl::TileLinkUH&);
	template auto upstream(scl::TileLinkChannelA&);
	template auto upstream(scl::TileLinkChannelD&);
	template auto upstream(const scl::TileLinkUL&);
	template auto upstream(const scl::TileLinkUH&);
	template auto upstream(const scl::TileLinkChannelA&);
	template auto upstream(const scl::TileLinkChannelD&);

	template auto downstream(scl::TileLinkUL&);
	template auto downstream(scl::TileLinkUH&);
	template auto downstream(scl::TileLinkChannelA&);
	template auto downstream(scl::TileLinkChannelD&);
	template auto downstream(const scl::TileLinkUL&);
	template auto downstream(const scl::TileLinkUH&);
	template auto downstream(const scl::TileLinkChannelA&);
	template auto downstream(const scl::TileLinkChannelD&);
}
