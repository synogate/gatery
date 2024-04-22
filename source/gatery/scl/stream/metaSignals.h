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
#include "StreamConcept.h"
#include "../flag.h"
#include "../utils/Thermometric.h"

namespace gtry::scl::strm {
	using std::move;

	struct Ready;
	struct Valid;
	struct ByteEnable;
	struct DwordEnable;
	struct Error;
	struct TxId;
	struct Sop;
	struct Eop;
	struct Empty;
	struct EmptyBits;

	/**
	* @brief High when all transfer conditions (ready and valid high) are met.
	* @param stream to test
	* @return transfer occurring
	*/
	template<StreamSignal StreamT> Bit transfer(const StreamT& stream) { return valid(stream) & ready(stream); }

	/**
	* @brief High when sink can accept incoming data.
	* @param stream to test
	* @return sink ready
	*/
	template<StreamSignal StreamT> const Bit ready(const StreamT& stream) { return '1'; }

	/**
	* @brief High when source has data to send.
	* @param stream to test
	* @return source has data
	*/
	const Bit valid(const StreamSignal auto& stream) { return '1'; }
	const Bit eop(const StreamSignal auto& stream) { return '1'; }
	const Bit sop(const StreamSignal auto& stream) { return '1'; }
	const UInt byteEnable(const StreamSignal auto& stream) { return ConstUInt(1, 1_b); }
	const BVec dwordEnable(const StreamSignal auto& stream) { return ConstBVec(1, 1_b); }
	const Bit error(const StreamSignal auto& stream) { return '0'; }
	const UInt txid(const StreamSignal auto& stream) { return 0; }

	struct Ready
	{
		Reverse<Bit> ready;
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Ready>())
		Bit& ready(StreamT& stream) { return *stream.template get<Ready>().ready; }
	template<StreamSignal StreamT> requires (StreamT::template has<Ready>())
		const Bit& ready(const StreamT& stream) { return *stream.template get<Ready>().ready; }


	struct Valid
	{
		// reset to zero
		Bit valid = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Valid>())
		Bit& valid(StreamT& stream) { return stream.template get<Valid>().valid; }
	template<StreamSignal StreamT> requires (StreamT::template has<Valid>())
		const Bit& valid(const StreamT& stream) { return stream.template get<Valid>().valid; }


	struct ByteEnable
	{
		BVec byteEnable;
	};
	template<StreamSignal StreamT> requires (StreamT::template has<ByteEnable>())
		BVec& byteEnable(StreamT& stream) { return stream.template get<ByteEnable>().byteEnable; }
	template<StreamSignal StreamT> requires (StreamT::template has<ByteEnable>())
		const BVec& byteEnable(const StreamT& stream) { return stream.template get<ByteEnable>().byteEnable; }

	struct DwordEnable
	{
		BVec dwordEnable;
	};
	template<StreamSignal T> requires (T::template has<DwordEnable>())
		BVec& dwordEnable(T& stream) { return stream.template get<DwordEnable>().dwordEnable; }
	template<StreamSignal T> requires (T::template has<DwordEnable>())
		const BVec& dwordEnable(const T& stream) { return stream.template get<DwordEnable>().dwordEnable; }

	struct Error
	{
		Bit error;
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Error>())
		Bit& error(StreamT& stream) { return stream.template get<Error>().error; }
	template<StreamSignal StreamT> requires (StreamT::template has<Error>())
		const Bit& error(const StreamT& stream) { return stream.template get<Error>().error; }


	struct TxId
	{
		UInt txid;
	};

	template<StreamSignal StreamT> requires (StreamT::template has<TxId>())
		UInt& txid(StreamT& stream) { return stream.template get<TxId>().txid; }
	template<StreamSignal StreamT> requires (StreamT::template has<TxId>())
		const UInt& txid(const StreamT& stream) { return stream.template get<TxId>().txid; }

	struct Eop
	{
		Bit eop;
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Eop>())
		Bit& eop(StreamT& stream) { return stream.template get<Eop>().eop; }
	template<StreamSignal StreamT> requires (StreamT::template has<Eop>())
		const Bit& eop(const StreamT& stream) { return stream.template get<Eop>().eop; }
	template<StreamSignal StreamT> requires (StreamT::template has<Valid>() and StreamT::template has<Eop>())
		const Bit sop(const StreamT& signal) { return !flag(transfer(signal), transfer(signal) & eop(signal)); }


	struct Sop
	{
		// reset to zero, sop is used for packet streams without valid.
		Bit sop = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Sop>())
		Bit& sop(StreamT& stream) { return stream.template get<Sop>().sop; }
	template<StreamSignal StreamT> requires (StreamT::template has<Sop>())
		const Bit& sop(const StreamT& stream) { return stream.template get<Sop>().sop; }
	template<StreamSignal StreamT> requires (!StreamT::template has<Valid>() and StreamT::template has<Sop>() and StreamT::template has<Eop>())
		const Bit valid(const StreamT& signal) { return flag(sop(signal) & ready(signal), eop(signal) & ready(signal)) | sop(signal); }


	struct Empty
	{
		UInt empty; // number of empty symbols in this beat
	};

	template<StreamSignal StreamT> requires (StreamT::template has<Empty>())
		UInt& empty(StreamT& stream) { return stream.template get<Empty>().empty; }
	template<StreamSignal StreamT> requires (StreamT::template has<Empty>())
		const UInt& empty(const StreamT& stream) { return stream.template get<Empty>().empty; }


	// for internal use only, we should introduce a symbol width and remove this
	struct EmptyBits
	{
		UInt emptyBits;
	};

	template<StreamSignal StreamT>
	const UInt emptyBits(const StreamT& in) { return ConstUInt(0, BitWidth::count(in->width().bits())); }

	template<StreamSignal StreamT> requires (not StreamT::template has<EmptyBits>() and StreamT::template has<Empty>())
	const UInt emptyBits(const StreamT& in) { return cat(empty(in), "b000"); }
	
	template<StreamSignal StreamT> requires (StreamT::template has<EmptyBits>())
	UInt& emptyBits(StreamT& in) { return in.template get<EmptyBits>().emptyBits; }
	
	template<StreamSignal StreamT> requires (StreamT::template has<EmptyBits>())
	const UInt& emptyBits(const StreamT& in) { return in.template get<EmptyBits>().emptyBits; }




	template<StreamSignal StreamT>
	const BVec emptyMask(const StreamT& in) { return ~ConstBVec(0, in->width()); }

	template<StreamSignal StreamT> requires (not StreamT::template has<EmptyBits>() and StreamT::template has<Empty>())
	const BVec emptyMask(const StreamT& in) { return emptyMaskGenerator(in.template get<Empty>().empty, 8_b, in->width()); }
	
	template<StreamSignal StreamT> requires (StreamT::template has<EmptyBits>())
	const BVec emptyMask(const StreamT& in) { return emptyMaskGenerator(in.template get<EmptyBits>().emptyBits, 1_b, in->width()); }




	template<StreamSignal StreamT> 
	auto simuReady(const StreamT &stream) {
		if constexpr (StreamT::template has<Ready>())
			return simu(ready(stream));
		else
			return '1';
	}

	template<StreamSignal StreamT> 
	auto simuValid(const StreamT &stream) {
		if constexpr (StreamT::template has<Valid>())
			return simu(valid(stream));
		else
			return '1';
	}

	template<StreamSignal StreamT>
	auto simuSop(const StreamT& stream) {
		if constexpr (StreamT::template has<Sop>())
			return simu(sop(stream));
		else
			return '1';
	}

	template<StreamSignal StreamT>
	auto simuEop(const StreamT& stream) {
		if constexpr (StreamT::template has<Eop>())
			return simu(eop(stream));
		else
			return '1';
	}
}


namespace gtry::scl {
	using strm::move;

	using strm::valid;
	using strm::ready;
	using strm::transfer;

	using strm::Ready;
	using strm::Valid;
	using strm::ByteEnable;
	using strm::DwordEnable;
	using strm::Error;
	using strm::TxId;
	using strm::Sop;
	using strm::Eop;
	using strm::Empty;
	using strm::EmptyBits;
}


BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Ready, ready);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Valid, valid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::ByteEnable, byteEnable);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::DwordEnable, dwordEnable);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Error, error);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::TxId, txid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Sop, sop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Eop, eop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Empty, empty);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::EmptyBits, emptyBits);
