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

#include "../stream/Stream.h"

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
	};

	struct AxiAddress
	{
		BVec id;
		UInt addr;
		UInt len = 8_b;
		UInt size = 3_b;
		BVec burst = 2_b;
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
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiAddress, id, addr, len, size, burst, cache, prot, qos, region, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiWriteData, data, strb, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiWriteResponse, id, resp, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiReadData, id, data, resp, user);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Axi4, ar, aw, w, b, r);
