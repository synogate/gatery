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
#include "axi.h"

namespace gtry::scl
{
	Axi4 Axi4::fromConfig(const AxiConfig& cfg)
	{
		Axi4 axi;

		(*axi.ar)->id = cfg.idW;
		(*axi.ar)->addr = cfg.addrW;
		(*axi.ar)->user = cfg.arUserW;

		(*axi.aw)->id = cfg.idW;
		(*axi.aw)->addr = cfg.addrW;
		(*axi.aw)->user = cfg.awUserW;

		(*axi.w)->data = cfg.dataW;
		(*axi.w)->strb = cfg.dataW / 8;
		(*axi.w)->user = cfg.wUserW;

		axi.b->id = cfg.idW;
		axi.b->user = cfg.bUserW;

		axi.r->id = cfg.idW;
		axi.r->data = cfg.dataW;
		axi.r->user = cfg.rUserW;

		return axi;
	}

	AxiConfig Axi4::config() const
	{
		return {
			.addrW = (*ar)->addr.width(),
			.dataW = (*w)->data.width(),
			.idW = (*ar)->id.width(),
			.arUserW = (*ar)->user.width(),
			.awUserW = (*aw)->user.width(),
			.wUserW = (*w)->user.width(),
			.bUserW = b->user.width(),
			.rUserW = r->user.width(),
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
		valid(*axi.aw) = '0';
		valid(*axi.w) = '0';
		ready(axi.b) = '1';
	}

	void axiDisableReads(Axi4& axi)
	{
		valid(*axi.ar) = '0';
		ready(axi.r) = '1';
	}
}