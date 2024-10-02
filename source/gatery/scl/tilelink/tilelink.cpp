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
#include "tilelink.h"

namespace gtry::scl
{
	Bit TileLinkA::hasData() const 
	{ 
		return !opcode.msb(); 
	}

	Bit TileLinkA::isGet() const 
	{ 
		return opcode == (size_t)TileLinkA::Get; 
	}

	Bit TileLinkA::isPut() const 
	{ 
		return opcode.upper(2_b) == 0; 
	}

	Bit TileLinkA::isBurst() const
	{
		size_t bytePerBeat = utils::Log2(data.width().bytes());
		return size > bytePerBeat & hasData();
	}

	void TileLinkA::setupGet(UInt address, UInt source, std::optional<UInt> size)
	{
		this->opcode = (size_t)Get;
		this->param = 0;
		this->source = zext(source);
		this->address = zext(address);

		if (size)
		{
			this->size = *size;
			this->mask = fullByteEnableMask(*this);
		}
		else
		{
			this->size = utils::Log2C(data.width().bytes());
			this->mask = (BVec)oext(0);
		}

		this->data = ConstBVec(this->data.width());
	}

	void TileLinkA::setupPut(UInt address, BVec data, UInt source, std::optional<UInt> size)
	{
		this->opcode = (size_t)PutFullData;
		this->param = 0;
		this->source = zext(source);
		this->address = zext(address);

		if (size)
		{
			this->size = *size;
			this->mask = fullByteEnableMask(*this);
		}
		else
		{
			this->size = utils::Log2C(this->data.width().bytes());
			this->mask = (BVec)oext(0);
		}

		this->data = data;
	}

	void TileLinkA::setupPutPartial(UInt address, BVec data, BVec mask, UInt source, std::optional<UInt> size)
	{
		this->opcode = (size_t)PutPartialData;
		this->param = 0;

		if (size)
			this->size = *size;
		else
			this->size = utils::Log2C(this->data.width().bytes());

		this->source = zext(source);
		this->address = zext(address);
		this->mask = mask;
		this->data = data;
	}

	Bit TileLinkD::hasData() const
	{ 
		return opcode.lsb(); 
	}

	Bit TileLinkD::isBurst() const
	{
		size_t bytePerBeat = utils::Log2(data.width().bytes());
		return size > bytePerBeat & hasData();
	}

	template struct strm::Stream<TileLinkA, Ready, Valid>;
	template struct strm::Stream<TileLinkD, Ready, Valid>;

	template UInt transferLength(const TileLinkChannelA&);
	template UInt transferLength(const TileLinkChannelD&);
	template std::tuple<Sop, Eop> seop(const TileLinkChannelA&);
	template std::tuple<Sop, Eop> seop(const TileLinkChannelD&);
	template Bit sop(const TileLinkChannelA&);
	template Bit sop(const TileLinkChannelD&);
	template Bit eop(const TileLinkChannelA&);
	template Bit eop(const TileLinkChannelD&);

	BVec fullByteEnableMask(const UInt& address, const UInt& size, BitWidth maskW)
	{
		BVec mask = ConstBVec(maskW);
		mask = (BVec)oext(0);

		const UInt& offset = address(0, BitWidth::count(maskW.bits()));
		for (size_t i = 0; (1ull << i) < maskW.bits(); i++)
		{
			IF(size == i)
			{
				mask = (BVec)zext(0);
				mask(offset, BitWidth{ 1ull << i }) = (BVec)sext(1);
			}
		}
		return mask;
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
			d = scl::strm::regDownstreamBlocking(move(d), { .allowRetimingBackward = true });

		ready(link.a) = ready(d);
		*link.d <<= strm::regReady(move(d));

		AddressSpaceDescriptionHandle desc = std::make_shared<AddressSpaceDescription>();
		desc->size = link.a->address.width() * 8_b;
		desc->name = mem.name();
		connectAddrDesc(link.addrSpaceDesc, desc);
	}
}

namespace gtry 
{
	template class Reverse<scl::TileLinkChannelD>;
}


GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::TileLinkD)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::TileLinkA)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::TileLinkChannelA)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::TileLinkChannelD)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUL)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUB)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUH)
