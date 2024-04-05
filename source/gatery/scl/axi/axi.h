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
#pragma once
#include <gatery/frontend.h>
#include <gatery/frontend/CompoundTemplateInstantiations.h>

#include "../stream/Stream.h"
#include "../stream/utils.h"

namespace gtry::scl
{
	enum class AxiResponseCode
	{
		OKAY, EXOKAY, SLVERR, DECERR
	};

	enum class AxiBurstType
	{
		FIXED, INCR, WRAP
	};

	struct AxiConfig
	{
		BitWidth addrW;
		BitWidth dataW;
		BitWidth idW;

		BitWidth arUserW;
		BitWidth awUserW;
		BitWidth wUserW;
		BitWidth bUserW;
		BitWidth rUserW;

		BitWidth alignedDataW() const { return BitWidth{ utils::nextPow2(dataW.bits()) }; }
		BitWidth wordAddrW() const { return addrW - BitWidth::count(dataW.bytes()); }
	};

	struct AxiAddress
	{
		BVec id;
		UInt addr;
		UInt len = 8_b;		// len + 1 = number of beats
		UInt size = 3_b;	// 2^size = number of bytes in one beat
		BVec burst = 2_b;	// burst address type
		BVec cache = 4_b;
		BVec prot = 3_b;
		UInt qos = 4_b;
		BVec region = 4_b;
		BVec user;
	};

	struct AxiWriteData
	{
		BVec data;
		BVec strb;
		BVec user;
	};

	struct AxiWriteResponse
	{
		BVec id;
		BVec resp = 2_b;
		BVec user;
	};

	struct AxiReadData
	{
		BVec id;
		BVec data;
		BVec resp = 2_b;
		BVec user;
	};

	struct Axi4
	{
		Reverse<RvStream<AxiAddress>> ar;
		Reverse<RvStream<AxiAddress>> aw;
		Reverse<RvPacketStream<AxiWriteData>> w;
		RvStream<AxiWriteResponse> b;
		RvPacketStream<AxiReadData> r;

		static Axi4 fromConfig(const AxiConfig& cfg);

		template<Signal T>
		static Axi4 fromMemory(Memory<T>& mem, BitWidth idW = 0_b);

		AxiConfig config() const;
	};

	void axiDisableWrites(Axi4& axi);
	void axiDisableReads(Axi4& axi);

	UInt burstAddress(const UInt& beat, const UInt& startAddr, const UInt& size, const BVec& burst);
	RvPacketStream<AxiAddress> axiAddBurst(RvStream<AxiAddress>&& req);

	scl::Axi4 axiRegDecouple(scl::Axi4&& slave, const RegisterSettings& settings = {});
	scl::Axi4 padWriteChannel(scl::Axi4& slave, BitWidth paddedW);

	enum AxiChannel{
		AC_READ  = 1,
		AC_WRITE = 2,
	};
	scl::Axi4 constrainAddressSpace(scl::Axi4&& slave, BitWidth addressW, const UInt& addressOffset, size_t channel = AC_READ | AC_WRITE);

	template<Signal T> RvPacketStream<AxiReadData> connectMemoryReadPort(Memory<T>& mem, RvStream<AxiAddress>&& req);
	template<Signal T> RvStream<AxiWriteResponse> connectMemoryWritePort(Memory<T>& mem, RvStream<AxiAddress>&& req, RvPacketStream<AxiWriteData>&& data);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiAddress, id, addr, len, size, burst, cache, prot, qos, region, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiWriteData, data, strb, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiWriteResponse, id, resp, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiReadData, id, data, resp, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Axi4, ar, aw, w, b, r);

namespace gtry::scl
{
	template<Signal T>
	inline Axi4 Axi4::fromMemory(Memory<T>& mem, BitWidth idW)
	{
		Area ent{ "scl_axi_fromMemory", true };

		BitWidth dataW = width(mem.defaultValue());
		Axi4 axi = Axi4::fromConfig({
			.addrW = mem.addressWidth() + BitWidth::count(dataW.bytes()),
			.dataW = dataW,
			.idW = idW,
		});
		axi.r = connectMemoryReadPort(mem, move(*axi.ar));
		axi.b = connectMemoryWritePort(mem, move(*axi.aw), move(*axi.w));
		return axi;
	}

	template<Signal T>
	inline RvPacketStream<AxiReadData> connectMemoryReadPort(Memory<T>& mem, RvStream<AxiAddress>&& req)
	{
		RvPacketStream<AxiReadData> resp = axiAddBurst(move(req)).transform([&](const AxiAddress& ar) {
			BitWidth dataW = width(mem.defaultValue());
			BitWidth wordAddrW = BitWidth::count(dataW.bytes());

			UInt wordAddr = ar.addr.upper(-wordAddrW);
			return AxiReadData{
				.id = ar.id,
				.data = (BVec)pack(mem[wordAddr]),
				.resp = ConstBVec((size_t)AxiResponseCode::OKAY, 2_b),
				.user = BVec{0}
			};
		});

		for (size_t i = 0; i < mem.readLatencyHint(); ++i)
			resp = strm::regDownstreamBlocking(move(resp), { .allowRetimingBackward = true });
		
		return resp;
	}

	template<Signal T>
	RvStream<AxiWriteResponse> connectMemoryWritePort(Memory<T>& mem, RvStream<AxiAddress>&& req, RvPacketStream<AxiWriteData>&& data)
	{
		RvStream<AxiWriteResponse> out;

		RvPacketStream<AxiAddress> burstReq = axiAddBurst(move(req));
		HCL_NAMED(burstReq);
		ready(burstReq) = ready(out) & valid(data);
		ready(data) = ready(out) & valid(burstReq);
		sim_assert(!valid(burstReq) | eop(burstReq) == eop(data)) << __FILE__ << " " << __LINE__;

		T unpackedData = constructFrom(mem.defaultValue());
		unpack(data->data.lower(width(unpackedData)), unpackedData);

		BitWidth wordAddrW = BitWidth::count(data->data.width().bytes());
		IF(transfer(data))
			mem[burstReq->addr.upper(-wordAddrW)] = unpackedData;

		valid(out) = valid(burstReq) & eop(burstReq);
		out->id = burstReq->id;
		out->resp = (size_t)AxiResponseCode::OKAY;
		out->user = 0;
		return out;
	}
}

GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::AxiAddress)
GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::AxiWriteResponse)
GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::AxiWriteData)
GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::AxiReadData)
GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::Axi4)


GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::strm::RvStream<gtry::scl::AxiAddress>)
GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiAddress>)
GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiWriteData>)
GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::strm::RvStream<gtry::scl::AxiWriteResponse>)
GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::scl::AxiReadData>)
