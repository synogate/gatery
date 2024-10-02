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
#include "../stream/utils.h"
#include "../stream/Packet.h"
#include "../utils/OneHot.h"
#include "../memoryMap/AddressSpaceDescription.h"

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

		void setupGet(UInt address, UInt source = 0, std::optional<UInt> size = {});
		void setupPut(UInt address, BVec data, UInt source = 0, std::optional<UInt> size = {});
		void setupPutPartial(UInt address, BVec data, BVec mask, UInt source = 0, std::optional<UInt> size = {});
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
			(Reverse<TileLinkChannelD>, d),
		
			/// Optional, human readable description of the address space to allow interconnects to compose those descriptions.
			/// Technically this is a Reverse<...> but we handle that with custom code in connect(...)
			(AddressSpaceDescriptionHandle, addrSpaceDesc)
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
	concept TileLinkSignal = Signal<T> and internal::is_tilelink_signal<std::remove_cvref_t<T>>::value;

	struct TileLinkCapBurst;
	struct TileLinkCapHint;
	struct TileLinkCapAtomicArith;
	struct TileLinkCapAtomicLogic;

	using TileLinkUL = TileLinkU<>;
	using TileLinkUB = TileLinkU<TileLinkCapBurst>;
	using TileLinkUH = TileLinkU<TileLinkCapBurst, TileLinkCapHint, TileLinkCapAtomicArith, TileLinkCapAtomicLogic>;

	inline UInt& txid(TileLinkChannelA& stream) { return stream->source; }
	inline const UInt& txid(const TileLinkChannelA& stream) { return stream->source; }
	inline UInt& txid(TileLinkChannelD& stream) { return stream->source; }
	inline const UInt& txid(const TileLinkChannelD& stream) { return stream->source; }

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

	BVec fullByteEnableMask(const UInt& address, const UInt& size, BitWidth maskW);
	inline BVec fullByteEnableMask(const TileLinkA& a) { return fullByteEnableMask(a.address, a.size, a.mask.width()); }
	inline void setFullByteEnableMask(TileLinkChannelA& a) { a->mask = fullByteEnableMask(*a); }

	UInt transferLengthFromLogSize(const UInt& logSize, size_t numSymbolsPerBeat);

	template<TileLinkSignal TLink>
	BVec responseOpCode(const TLink& link);

	template<TileLinkSignal TLink>
	void tileLinkInit(TLink& link, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth);

	TileLinkD tileLinkDefaultResponse(const TileLinkA& request);

	using gtry::connect;
	void connect(Memory<BVec>& mem, TileLinkUL& link);

	template<TileLinkSignal TLink> TLink regDecouple(TLink&& link);
	/**
	 * @brief	Places stream decoupling registers in both the A channel direction and the D channel direction. Quick and sometimes dirty way to add
	 *			pipelining registers to a design which already features tileLinks. This function consumes a slave-side tileLink and returns a master-side tileLink.
	 * @param slave the slave-side tileLink
	 * @return the slave-side tileLink handle, where decoupling registers have been inserted in both directions.
	*/
	template <TileLinkSignal TLink> TLink tileLinkRegDecouple(TLink&& slave);

	/// Overload to correctly forward addr space description
	template<TileLinkSignal T>
	void connect(T& lhs, T& rhs);
}



namespace gtry 
{
	extern template class Reverse<scl::TileLinkChannelD>;

	// For some reason, the compound constructFrom from ConstructFrom.h does not pick up the constructFrom of AddressSpaceDescriptionHandle
	// so as a workaround we provide an overload for the entire tilelink struct that picks things up correctly.
	// It also needs to be defined before the use of constructFrom in tileLinkRegDecouple.
	template<class... Capability>
	inline scl::TileLinkU<Capability...> constructFrom(const scl::TileLinkU<Capability...>& val) { 
		scl::TileLinkU<Capability...> res;
		res.a = constructFrom(val.a);
		res.d = constructFrom(val.d);
		res.addrSpaceDesc = constructFrom(val.addrSpaceDesc);
		return res;
	}
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
		return { Sop{start}, Eop{end} };
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

	template<TileLinkSignal TLink>
	BVec responseOpCode(const TLink& link)
	{
		BVec op = 3_b;
		op = (size_t)TileLinkD::AccessAckData;

		IF(link.a->opcode(1, 2_b) == 0) // PutFull & PuPartial
			op = (size_t)TileLinkD::AccessAck;

		if(TLink::template capability<TileLinkCapHint>())
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

		link.addrSpaceDesc = std::make_shared<AddressSpaceDescription>();
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
	TLink regDecouple(TLink&& link)
	{
		TLink out{
			.a = regDecouple(link.a),
			.d = constructFrom(*link.d)
		};
		*link.d <<= regDecouple(*out.d);
		
		out.addrSpaceDesc = link.addrSpaceDesc;
		return out;
	}


	template <TileLinkSignal TLink>
	TLink tileLinkRegDecouple(TLink&& slave)
	{
		TLink master = constructFrom(slave);
		TLink masterTemp = constructFrom(master);
		masterTemp <<= master;
		slave <<= regDecouple(move(masterTemp));
		return master;
	}

	template<TileLinkSignal T>
	void connect(T& lhs, T& rhs)
	{
		using namespace gtry;
		auto lhsAddrSpaceBefore = lhs.addrSpaceDesc;
		auto rhsAddrSpaceBefore = rhs.addrSpaceDesc;
		downstream(lhs) = downstream(rhs);
		upstream(rhs) = upstream(lhs);
		lhs.addrSpaceDesc = lhsAddrSpaceBefore;
		rhs.addrSpaceDesc = rhsAddrSpaceBefore;
		connectAddrDesc(rhs.addrSpaceDesc, lhs.addrSpaceDesc);
	}

	extern template struct strm::Stream<TileLinkA, Ready, Valid>;
	extern template struct strm::Stream<TileLinkD, Ready, Valid>;

	extern template UInt transferLength(const TileLinkChannelA&);
	extern template UInt transferLength(const TileLinkChannelD&);
	extern template std::tuple<Sop, Eop> seop(const TileLinkChannelA&);
	extern template std::tuple<Sop, Eop> seop(const TileLinkChannelD&);
	extern template Bit sop(const TileLinkChannelA&);
	extern template Bit sop(const TileLinkChannelD&);
	extern template Bit eop(const TileLinkChannelA&);
	extern template Bit eop(const TileLinkChannelD&);
}



BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkA, opcode, param, size, source, address, mask, data);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkD, opcode, param, size, source, sink, data, error);

GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::TileLinkD);
GTRY_EXTERN_TEMPLATE_COMPOUND(gtry::scl::TileLinkA);

GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::TileLinkChannelA);
GTRY_EXTERN_TEMPLATE_STREAM(gtry::scl::TileLinkChannelD);
GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUL);
GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUB);
GTRY_EXTERN_TEMPLATE_COMPOUND_MINIMAL(gtry::scl::TileLinkUH);
