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
	};
	using TileLinkChannelA = RvStream<BVec, TileLinkA, ByteEnable>;

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
	};
	using TileLinkChannelD = RvStream<BVec, TileLinkD, Error>;

	template<class... Capability>
	struct TileLinkU
	{
		BOOST_HANA_DEFINE_STRUCT(TileLinkU,
			(TileLinkChannelA, a),
			(TileLinkChannelD, d)
		);

		TileLinkA& chanA() { return a.template get<TileLinkA>(); }
		const TileLinkA& chanA() const { return a.template get<TileLinkA>(); }
		TileLinkD& chanD() { return d.template get<TileLinkD>(); }
		const TileLinkD& chanD() const { return d.template get<TileLinkD>(); }

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
	using TileLinkUH = TileLinkU<TileLinkCapBurst, TileLinkCapHint, TileLinkCapAtomicArith, TileLinkCapAtomicLogic>;

	template<class... Cap>
	void connect(TileLinkU<Cap...>& lhs, TileLinkU<Cap...>& rhs);


	template<StreamSignal T>
	requires(T::template has<TileLinkA>())
	UInt transferLength(const T& source);

	template<StreamSignal T>
	requires(T::template has<TileLinkD>())
	UInt transferLength(const T& source);

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	std::tuple<Sop, Eop> seop(T& source);

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit sop(T& source);

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit eop(T& source);
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

	template<class ...Cap>
	void connect(TileLinkU<Cap...>& lhs, TileLinkU<Cap...>& rhs)
	{
		HCL_DESIGNCHECK(lhs.a->width() == rhs.a->width());
		HCL_DESIGNCHECK(lhs.d->width() == rhs.d->width());
		HCL_DESIGNCHECK(lhs.chanA().address.width() == rhs.chanA().address.width());
		HCL_DESIGNCHECK(byteEnable(lhs.a).width() == byteEnable(rhs.a).width());

		lhs.a <<= rhs.a;
		rhs.d <<= lhs.d;
	}

	inline void setFullByteEnableMask(TileLinkChannelA& a)
	{
		BVec& be = byteEnable(a);
		be = (BVec)sext(1);

		const UInt& size = a.template get<TileLinkA>().size;
		const UInt& offset = a.template get<TileLinkA>().address(0, BitWidth::count(be.width().bits()));
		for (size_t i = 0; (1ull << i) < be.width().bits(); i++)
		{
			IF(size == i)
			{
				be = (BVec)zext(0);
				be(offset, BitWidth{ 1ull << i }) = (BVec)sext(1);
			}
		}
	}

	inline UInt transferLengthFromLogSize(const UInt& logSize, size_t numSymbolsPerBeat)
	{
		BitWidth beatWidth = BitWidth::count(numSymbolsPerBeat);
		UInt size = decoder(logSize);
		UInt beats = size.upper(size.width() - beatWidth);
		beats.lsb() |= size.lower(beatWidth) != 0;
		return beats;
	}

	template<StreamSignal T>
	requires(T::template has<TileLinkA>())
	UInt transferLength(const T& source)
	{
		UInt len = transferLengthFromLogSize(source.template get<TileLinkA>().size, byteEnable(source).width().bits());
		IF(source.template get<TileLinkA>().opcode.upper(2_b) != 0)
			len = 1; // only puts are multi beat
		return len;
	}

	template<StreamSignal T>
	requires(T::template has<TileLinkD>())
	UInt transferLength(const T& source)
	{
		UInt len = transferLengthFromLogSize(source.template get<TileLinkD>().size, byteEnable(source).width().bits());
		IF(!source.template get<TileLinkD>().opcode.lsb())
			len = 1; // only data responses are multi beat
		return len;
	}

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	std::tuple<Sop, Eop> seop(T& source)
	{
		auto scope = Area{ "scl_seop" }.enter();

		UInt size = transferLength(source);
		HCL_NAMED(size);

		UInt beatCounter = size.width();
		UInt beatCounterNext = beatCounter + 1;

		Bit start;
		IF(transfer(source))
		{
			sim_assert(size != 0) << "what is a zero length packet?";
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
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit sop(T& source)
	{
		auto [s, e] = seop(source);
		return s.sop;
	}

	template<StreamSignal T>
	requires requires (T& s) { { transferLength(s) } -> std::convertible_to<UInt>; }
	Bit eop(T& source)
	{
		auto [s, e] = seop(source);
		return e.eop;
	}

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkA, opcode, param, size, source, address);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkD, opcode, size, source);
