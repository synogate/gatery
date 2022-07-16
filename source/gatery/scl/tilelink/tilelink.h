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
	struct TileLinkA
	{
		enum OpCode {
			PutFullData = 0,	// UL
			PutPartialData = 1,	// UL
			ArithmeticData = 2,	// UH
			LogicalData = 3,	// UH
			Get = 4,			// UL
			Intent = 5,			// UH
			Acquire = 6,		// C
		};

		enum OpCodeParam {
			// ArithmeticData
			MIN = 0,
			MAX = 1,
			MINU = 2,
			MAXU = 3,
			ADD = 4,

			// LogicalData
			XOR = 0,
			OR = 1,
			AND = 2,
			SWAP = 3,

			// Intent
			PrefetchRead = 0,
			PrefetchWrite = 1,
		};

		BVec opcode = 3_b;
		BVec param = 3_b;
		UInt size;
		BVec source;
		BVec address;
		// use ByteEnable for mask
		BVec data;
	};
	using TileLinkChannelA = RvStream<TileLinkA, ByteEnable>;

	struct TileLinkD
	{
		enum OpCode {
			AccessAck = 0,		// UL
			AccessAckData = 1,	// UL
			HintAck = 2,		// UH
			Grant = 4,			// C
			GrantData = 5,		// C
			ReleaseAck = 6		// C
		};

		BVec opcode = 3_b;
		//BVec param = 3_b;
		UInt size;
		BVec source;
		//BVec sink;
		BVec data;
		// use Error for error
	};
	using TileLinkChannelD = RvStream<TileLinkD, Error>;

	template<class... Capability>
	struct TileLinkU
	{
		BOOST_HANA_DEFINE_STRUCT(TileLinkU,
			(TileLinkChannelA, a),
			(TileLinkChannelD, d)
		);

		template<class T>
		static constexpr bool capability();
	};

	struct TileLinkCapBurst;
	struct TileLinkCapHint;
	struct TileLinkCapAtomicArith;
	struct TileLinkCapAtomicLogic;

	using TileLinkUL = TileLinkU<>;
	using TileLinkUH = TileLinkU<TileLinkCapBurst, TileLinkCapHint, TileLinkCapAtomicArith, TileLinkCapAtomicLogic>;
}

// impl
namespace gtry::scl
{
	template<class ...Capability>
	template<class T>
	inline constexpr bool gtry::scl::TileLinkU<Capability...>::capability()
	{
		if constexpr (sizeof...(Capability) != 0)
			return (std::is_same_v<T, Capability> | ...);
		return false;
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkA, opcode, param, size, source, address, data);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkD, opcode, size, source, data);
