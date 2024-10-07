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
#include "axi.h"

namespace gtry::scl
{
	Axi4 Axi4::fromConfig(const AxiConfig& cfg)
	{
		Axi4 axi;

		(*axi.r.a)->id = cfg.idW;
		(*axi.r.a)->addr = cfg.addrW;
		(*axi.r.a)->user = cfg.arUserW;

		(*axi.w.a)->id = cfg.idW;
		(*axi.w.a)->addr = cfg.addrW;
		(*axi.w.a)->user = cfg.awUserW;

		(*axi.w.d)->data = cfg.dataW;
		(*axi.w.d)->strb = cfg.dataW / 8;
		(*axi.w.d)->user = cfg.wUserW;

		axi.w.b->id = cfg.idW;
		axi.w.b->user = cfg.bUserW;

		axi.r.d->id = cfg.idW;
		axi.r.d->data = cfg.dataW;
		axi.r.d->user = cfg.rUserW;

		return axi;
	}

	AxiConfig Axi4::config() const
	{
		HCL_DESIGNCHECK_HINT((*r.a)->addr.width() == (*w.a)->addr.width(), "you have a non-standard axi interface. It can not be reproduced through a config");
		return {
			.addrW = (*r.a)->addr.width(),
			.dataW = (*w.d)->data.width(),
			.idW = (*r.a)->id.width(),
			.arUserW = (*r.a)->user.width(),
			.awUserW = (*w.a)->user.width(),
			.wUserW = (*w.d)->user.width(),
			.bUserW = w.b->user.width(),
			.rUserW = r.d->user.width(),
		};
	}

	UInt burstAddress(const UInt& beat, const UInt& startAddr, const UInt& size, const BVec& burst)
	{
		UInt beatAddress = startAddr;
		IF(burst == (size_t)AxiBurstType::INCR)
			beatAddress |= zext(beat, startAddr.width()) << size;
		HCL_NAMED(beatAddress);
		return beatAddress;
	}

	RvPacketStream<AxiAddress> axiAddBurst(RvStream<AxiAddress>&& req)
	{
		RvPacketStream<AxiAddress> out;

		scl::Counter beatCtr{ req->len + 1 };
		IF(transfer(out))
			beatCtr.inc();

		*out = *req;
		out->addr = burstAddress(beatCtr.value(), req->addr, req->size, req->burst);
		valid(out) = valid(req);
		ready(req) = ready(out) & valid(req) & beatCtr.isLast();
		eop(out) = beatCtr.isLast();
		return out;
	}

	void axiDisableWrites(Axi4& axi)
	{
		valid(*axi.w.a) = '0';
		valid(*axi.w.d) = '0';
		ready(axi.w.b) = '1';
	}

	void axiDisableReads(Axi4& axi)
	{
		valid(*axi.r.a) = '0';
		ready(axi.r.d) = '1';
	}

	scl::Axi4 axiRegDecouple(scl::Axi4&& slave, const RegisterSettings& settings) {
		scl::Axi4 master = constructFrom(slave);

		*slave.w.a <<= scl::strm::regDecouple(*master.w.a, settings);
		*slave.w.d <<= scl::strm::regDecouple(*master.w.d, settings);
		*slave.r.a <<= scl::strm::regDecouple(*master.r.a, settings);

		master.r.d <<= scl::strm::regDecouple(slave.r.d, settings);
		master.w.b <<= scl::strm::regDecouple(slave.w.b, settings);

		return master;
	}

	scl::Axi4 padWriteChannel(scl::Axi4& slave, BitWidth paddedW) {
		auto cfg = slave.config();
		cfg.dataW = paddedW;

		scl::Axi4 master = scl::Axi4::fromConfig(cfg);

		*slave.w.a <<= *master.w.a;
		*slave.w.d <<= transform(move(*master.w.d),
			[&](const scl::AxiWriteData& awd) {
				return scl::AxiWriteData{
					.data = awd.data.lower((*slave.w.d)->data.width()),
					.strb = awd.strb.lower((*slave.w.d)->strb.width()),
					.user = awd.user,
				};
			});

		master.w.b <<= slave.w.b;

		return master;
	}

	scl::Axi4 constrainAddressSpace(scl::Axi4&& slave, BitWidth addressW, const UInt& addressOffset, size_t channels)
	{
		scl::Axi4 master = constructFrom(slave);
		master.r.d <<= slave.r.d;
		master.w.b <<= slave.w.b;
		*slave.w.d <<= *master.w.d;

		if (channels & AC_WRITE) {
			(*master.w.a)->addr.resetNode();
			(*master.w.a)->addr = addressW;
			HCL_DESIGNCHECK_HINT(addressW <= (*slave.w.a)->addr.width(), "you are trying to extend the address space instead of constraining it");
			*slave.w.a <<= transform(move(*master.w.a),
				[&](const scl::AxiAddress& aa) {
					scl::AxiAddress ret = aa;
					ret.addr.resetNode();
					ret.addr = zext(cat(addressOffset, aa.addr), (*slave.w.a)->addr.width());
					return ret;
				});
		}
		else
			*slave.w.a <<= *master.w.a;

		if (channels & AC_READ) {
			(*master.r.a)->addr.resetNode();
			(*master.r.a)->addr = addressW;
			HCL_DESIGNCHECK_HINT(addressW <= (*slave.r.a)->addr.width(), "you are trying to extend the address space instead of constraining it");
			*slave.r.a <<= transform(move(*master.r.a),
				[&](const scl::AxiAddress& aa) {
					scl::AxiAddress ret = aa;
					ret.addr.resetNode();
					ret.addr = zext(cat(addressOffset, aa.addr), (*slave.r.a)->addr.width());
					return ret;
				});
		}
		else
			*slave.r.a <<= *master.r.a;

		return master;
	}
}

GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::AxiAddress)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::AxiWriteResponse)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::AxiWriteData)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND(gtry::scl::AxiReadData)
GTRY_INSTANTIATE_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::Axi4)


GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvStream<gtry::scl::AxiAddress>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiAddress>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiWriteData>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvStream<gtry::scl::AxiWriteResponse>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiReadData>)
