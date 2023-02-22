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
#include "../stream/Packet.h"
#include "../utils/OneHot.h"

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
		UInt source;
		UInt address;
		BVec mask;
		BVec data;

		Bit hasData() const;
		Bit isGet() const;
		Bit isPut() const;
		Bit isBurst() const;
	};
	using TileLinkChannelA = RvStream<TileLinkA>;

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
		BVec param = 3_b;
		UInt size;
		UInt source;
		UInt sink;
		BVec data;
		Bit error;

		Bit hasData() const;
		Bit isBurst() const;
	};
	using TileLinkChannelD = RvStream<TileLinkD>;

	template<class... Capability>
	struct TileLinkU
	{
		BOOST_HANA_DEFINE_STRUCT(TileLinkU,
			(TileLinkChannelA, a),
			(Reverse<TileLinkChannelD>, d)
		);

		template<class T>
		static constexpr bool capability();
	};

	namespace internal
	{
		template<class T>		struct is_tilelink_signal : std::false_type {};
		template<class... T>	struct is_tilelink_signal<TileLinkU<T...>> : std::true_type {};
	}

	template<class T>
	concept TileLinkSignal = Signal<T> and internal::is_tilelink_signal<T>::value;

	struct TileLinkCapBurst;
	struct TileLinkCapHint;
	struct TileLinkCapAtomicArith;
	struct TileLinkCapAtomicLogic;

	using TileLinkUL = TileLinkU<>;
	using TileLinkUB = TileLinkU<TileLinkCapBurst>;
	using TileLinkUH = TileLinkU<TileLinkCapBurst, TileLinkCapHint, TileLinkCapAtomicArith, TileLinkCapAtomicLogic>;

	template<StreamSignal T>
	requires(T::template has<TileLinkA>())
	UInt transferLength(const T& source);

	template<StreamSignal T>
	requires(T::template has<TileLinkD>())
	UInt transferLength(const T& source);

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	std::tuple<Sop, Eop> seop(const T& source);

	template<StreamSignal T>
	requires requires (const T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit sop(const T& source);

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit eop(const T& source);

	void setFullByteEnableMask(TileLinkChannelA& a);
	UInt transferLengthFromLogSize(const UInt& logSize, size_t numSymbolsPerBeat);

	BVec responseOpCode(const TileLinkSignal auto& link);

	template<TileLinkSignal TLink>
	void tileLinkInit(TLink& link, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth);

	TileLinkD tileLinkDefaultResponse(const TileLinkA& request);

	using gtry::connect;
	void connect(Memory<BVec>& mem, TileLinkUL& link);

	template<TileLinkSignal TLink> TLink reg(TLink& link);
	template<TileLinkSignal TLink> TLink reg(TLink&& link);
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

	template<StreamSignal T>
	requires(T::template has<TileLinkA>())
	UInt transferLength(const T& source)
	{
		UInt len = transferLengthFromLogSize(source->size, source->mask.width().bits());
		IF(source->opcode.upper(2_b) != 0)
			len = 1; // only puts are multi beat
		return len;
	}

	template<StreamSignal T>
	requires(T::template has<TileLinkD>())
	UInt transferLength(const T& source)
	{
		UInt len = transferLengthFromLogSize(source->size, (source->data.width() / 8).bits());
		IF(!source->opcode.lsb())
			len = 1; // only data responses are multi beat
		return len;
	}

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	std::tuple<Sop, Eop> seop(const T& source)
	{
		auto scope = Area{ "scl_seop" }.enter();

		UInt size = transferLength(source);
		HCL_NAMED(size);

		UInt beatCounter = size.width();
		UInt beatCounterNext = beatCounter + 1;

		Bit start;
		IF(transfer(source))
		{
			//sim_assert(size != 0) << "what is a zero length packet?";
			start = '0';
			beatCounter = beatCounterNext;
		}

		Bit end = '0';
		IF(beatCounterNext == size)
		{
			end = '1';
			IF(transfer(source))
			{
				start = '1';
				beatCounter = 0;
			}
		}
		start = reg(start, '1');
		beatCounter = reg(beatCounter, 0);

		HCL_NAMED(beatCounter);
		HCL_NAMED(start);
		HCL_NAMED(end);
		return { Sop(start), Eop(end) };
	}

	template<StreamSignal T>
	requires requires (const T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit sop(const T& source)
	{
		auto [s, e] = seop(source);
		return s.sop;
	}

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit eop(const T& source)
	{
		auto [s, e] = seop(source);
		return e.eop;
	}

	BVec responseOpCode(const TileLinkSignal auto& link)
	{
		BVec op = 3_b;
		op = (size_t)TileLinkD::AccessAckData;

		IF(link.a->opcode(1, 2_b) == 0) // PutFull & PuPartial
			op = (size_t)TileLinkD::AccessAck;

		if(link.template capability<TileLinkCapHint>())
			IF(link.a->opcode == (size_t)TileLinkA::Intent)
				op = (size_t)TileLinkD::HintAck;

		return op;
	}

	template<TileLinkSignal TLink>
	void tileLinkInit(TLink& link, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth)
	{
		link.a->size = sizeWidth;
		link.a->source = sourceWidth;
		link.a->address = addrWidth;
		link.a->mask = dataWidth / 8;
		link.a->data = dataWidth;

		(*link.d)->data = dataWidth;
		(*link.d)->size = sizeWidth;
		(*link.d)->source = sourceWidth;
		(*link.d)->sink = 0_b;
	}

	template<TileLinkSignal TLink>
	TLink tileLinkInit(BitWidth addrW, BitWidth dataW, BitWidth sourceW = 0_b, std::optional<BitWidth> sizeW = std::nullopt)
	{
		if (!sizeW)
			sizeW = BitWidth::last(utils::Log2C(dataW.bytes()));

		TLink link;
		tileLinkInit(link, addrW, dataW, *sizeW, sourceW);
		return link;
	}

	template<TileLinkSignal TLink>
	TLink reg(TLink& link)
	{
		TLink out{
			.a = reg(link.a),
			.d = constructFrom(*link.d)
		};
		*link.d <<= reg(*out.d);
		return out;
	}
	
	template<TileLinkSignal TLink>
	TLink reg(TLink&& link) { return reg(link); }

	extern template struct Stream<TileLinkA, Ready, Valid>;
	extern template struct Stream<TileLinkD, Ready, Valid>;

	extern template UInt transferLength(const TileLinkChannelA&);
	extern template UInt transferLength(const TileLinkChannelD&);
	extern template std::tuple<Sop, Eop> seop(const TileLinkChannelA&);
	extern template std::tuple<Sop, Eop> seop(const TileLinkChannelD&);
	extern template Bit sop(const TileLinkChannelA&);
	extern template Bit sop(const TileLinkChannelD&);
	extern template Bit eop(const TileLinkChannelA&);
	extern template Bit eop(const TileLinkChannelD&);
}

namespace gtry 
{
	extern template class Reverse<scl::TileLinkChannelD>;

	extern template void connect(scl::TileLinkUL&, scl::TileLinkUL&);
	extern template void connect(scl::TileLinkUH&, scl::TileLinkUH&);
	extern template void connect(scl::TileLinkChannelA&, scl::TileLinkChannelA&);
	extern template void connect(scl::TileLinkChannelD&, scl::TileLinkChannelD&);

	extern template auto upstream(scl::TileLinkUL&);
	extern template auto upstream(scl::TileLinkUH&);
	extern template auto upstream(scl::TileLinkChannelA&);
	extern template auto upstream(scl::TileLinkChannelD&);
	extern template auto upstream(const scl::TileLinkUL&);
	extern template auto upstream(const scl::TileLinkUH&);
	extern template auto upstream(const scl::TileLinkChannelA&);
	extern template auto upstream(const scl::TileLinkChannelD&);

	extern template auto downstream(scl::TileLinkUL&);
	extern template auto downstream(scl::TileLinkUH&);
	extern template auto downstream(scl::TileLinkChannelA&);
	extern template auto downstream(scl::TileLinkChannelD&);
	extern template auto downstream(const scl::TileLinkUL&);
	extern template auto downstream(const scl::TileLinkUH&);
	extern template auto downstream(const scl::TileLinkChannelA&);
	extern template auto downstream(const scl::TileLinkChannelD&);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkA, opcode, param, size, source, address, mask, data);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkD, opcode, param, size, source, sink, data, error);
