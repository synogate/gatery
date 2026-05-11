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
#include "TileLinkAdapter.h"

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
		if (data.width() < 8_b)
			return hasData();

		size_t bytePerBeat = utils::Log2(data.width().bytes());
		return size > bytePerBeat & hasData();
	}

	UInt& TileLinkA::wordAddress()
	{
		return address.upper(-BitWidth::count(data.width().bytes()));
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

	TileLinkCfg TileLinkA::defaultCfg() const
	{
		TileLinkCfg cfg{
			.addrW = address.width(),
			.dataW = data.width(),
			.sourceW = source.width(),
			.burstLimitLog = size.width().last(),
		};
		cfg.opValid[TileLinkA::Get] = true;
		cfg.opValid[TileLinkA::PutFullData] = true;
		cfg.opValid[TileLinkA::PutPartialData] = true;
		return cfg;
	}

	TileLinkU tileLinkRegDecouple(TileLinkU&& slave)
	{
		TileLinkU master = constructFrom(slave);
		TileLinkU masterTemp = constructFrom(master);
		masterTemp <<= master;
		slave <<= regDecouple(move(masterTemp));
		return master;
	}

	TileLinkU tileLinkRegA(TileLinkU&& slave)
	{
		TileLinkU out = constructFrom(slave);
		slave.a <<= strm::regDecouple(move(out.a));
		*out.d <<= move(*slave.d);
		return out;
	}

	TileLinkU tileLinkRegD(TileLinkU&& slave)
	{
		TileLinkU out = constructFrom(slave);
		slave.a <<= move(out.a);
		*out.d <<= strm::regDecouple(move(*slave.d));
		return out;
	}

	Bit TileLinkD::hasData() const
	{ 
		return opcode.lsb(); 
	}

	Bit TileLinkD::isBurst() const
	{
		if (data.width() < 8_b)
			return hasData();

		size_t bytePerBeat = utils::Log2(data.width().bytes());
		return size > bytePerBeat & hasData();
	}

	Bit eop(const UInt& logByteSize, BitWidth bitPerBeat, Bit forceEop)
	{
		Area ent{ "scl_eop_logsize", true };

		BitWidth beatCounterW =
			BitWidth{ logByteSize.width().count() }
			- BitWidth::count(bitPerBeat.bits())
			+ 3;
		UInt beatCounter = beatCounterW;
		beatCounter += 1;

		UInt paddedBeatCounter = ConstUInt(BitWidth{ logByteSize.width().count() });
		BitWidth paddedBeatCounterUpperW = std::min(paddedBeatCounter.width(), beatCounter.width() - 1);
		if(paddedBeatCounterUpperW != paddedBeatCounter.width())
			paddedBeatCounter.lower(-paddedBeatCounterUpperW) |= '1';
		paddedBeatCounter.upper(paddedBeatCounterUpperW) = beatCounter.upper(paddedBeatCounterUpperW);
		HCL_NAMED(paddedBeatCounter);

		Bit end = paddedBeatCounter[logByteSize];
		end |= forceEop;
		HCL_NAMED(end);

		IF(end)
			beatCounter = 0;
		beatCounter = reg(beatCounter, 0) | name("beatCounter");
		return end;
	}

	Bit eop(const TileLinkChannel auto& source)
	{
		if (source->data.width() == 0_b)
			return '1';

		gtry::EnableScope en{ transfer(source) };
		return eop(source->size, source->data.width(), !source->hasData());
	}
	template Bit eop(const TileLinkChannelA&);
	template Bit eop(const TileLinkChannelD&);

	template struct strm::Stream<TileLinkA, Ready, Valid>;
	template struct strm::Stream<TileLinkD, Ready, Valid>;

	template UInt transferLength(const TileLinkChannelA&);
	template UInt transferLength(const TileLinkChannelD&);

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

	TileLinkU tileLinkInit(const TileLinkCfg& cfg)
	{
		const BitWidth sizeW = std::max(
			BitWidth::last(cfg.burstLimitLog),
			BitWidth::last(utils::Log2C(cfg.dataW.bytes()))
		);

		TileLinkU link = tileLinkInit<TileLinkU>(cfg.addrW, cfg.dataW, cfg.sourceW, sizeW);
		link.cfg = cfg;
		return link;
	}

	TileLinkU tileLinkSlaveInit(BitWidth addrW, BitWidth dataW, BitWidth sourceW)
	{
		TileLinkU link = tileLinkInit<TileLinkU>(addrW, dataW, sourceW);
		HCL_NAMED(link);

		**link.d = tileLinkDefaultResponse(*link.a);
		(*link.d)->data = dontCare(BVec{ dataW });

		valid(*link.d) = valid(link.a);
		ready(link.a) = ready(*link.d);
		return link;
	}

	void connect(Memory<BVec>& mem, TileLinkU& link)
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

	TileLinkU tileLinkMemory(BitWidth addrW, BitWidth dataW, BitWidth sourceW)
	{
		Area ent{ "scl_TLMemory", true };

		BitWidth memAddrW = addrW - BitWidth::count((dataW / 8).bits());
		Memory<BVec> mem(memAddrW.count(), dataW);
		auto link = tileLinkInit<TileLinkU>(addrW, dataW, sourceW);
		connect(mem, link);
		HCL_NAMED(link);
		return link;
	}

	TileLinkU tileLinkMemory(Memory<BVec>& mem, TileLinkMemoryConfig cfg)
	{
		BitWidth wordAddrW = BitWidth::count(mem.wordSize().bytes());
		TileLinkU link = tileLinkSlaveInit(mem.addressWidth() + wordAddrW, mem.wordSize(), cfg.sourceW);

		auto port = mem[link.a->address.upper(-wordAddrW)];

		if (cfg.writePort)
		{
			IF(transfer(link.a) & link.a->isPut())
			{
#if 0
				BVec newData = (BVec)pack(port.read());
				for (size_t i = 0; i < link.a->mask.size(); ++i)
					IF(link.a->mask[i])
					newData.part(link.a->mask.size(), i) = link.a->data.part(link.a->mask.size(), i);

				T writeData = constructFrom(mem.defaultValue());
				unpack(newData, writeData);
				port = writeData;
#else
				sim_assert(link.a->mask == link.a->mask.width().mask()) << "no write mask support work around";
				port = link.a->data;
#endif
			}
		}

		if (cfg.readPort)
		{
			(*link.d)->data = (BVec)pack(port.read());
		}

		size_t latency = cfg.latency.value_or(mem.readLatencyHint());
		for (size_t i = 0; i < latency && cfg.readPort; ++i)
			*link.d = strm::regDownstream(move(*link.d), { .allowRetimingBackward = true });
		return link;
	}

	TileLinkU tileLinkSimOverride(TileLinkU&& slave, TileLinkU&& simulationSlave)
	{
		TileLinkU master = constructFrom(slave);
		auto [exp, sim] = simOverrideUpstream(move(master));
		slave <<= exp;
		simulationSlave <<= sim;

		// mask all requests during simulation to prevent false errors in slave due to broken flow control
		valid(slave.a) = simOverride(valid(master.a), Bit{ '0' });
		return master;
	}

	TileLinkU tileLinkMemorySimulation(TileLinkU&& slave)
	{
		Area ent{ "scl_TLMemorySimulation", true };

		if (slave.cfg.burstSupport())
		{
			TileLinkU mem = tileLinkMemory(slave.cfg.addrW, slave.cfg.dataW, slave.cfg.sourceW + 7_b);
			TileLinkU burst = tileLinkAddBurst(move(mem), slave.cfg.burstLimitLog);
			return tileLinkSimOverride(move(slave), move(burst));
		}
		else
		{
			TileLinkU mem = tileLinkMemory(slave.cfg.addrW, slave.cfg.dataW, slave.cfg.sourceW);
			return tileLinkSimOverride(move(slave), move(mem));
		}
	}

	size_t TileLinkCfg::burstThresholdLog() const
	{
		return utils::Log2C((dataW.bits() + 7) / 8);
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
GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkU)
