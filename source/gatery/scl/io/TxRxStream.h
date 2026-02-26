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
	template<Signal T = BVec, Signal... MetaT>
	struct TxRxStreamBase
	{
		BOOST_HANA_DEFINE_STRUCT(TxRxStreamBase,
			(RvStream<T, MetaT...>, tx), 
			(Reverse<RvStream<T, MetaT...>>, rx)
		);
	};

	using TxRxStream = TxRxStreamBase<BVec>;
	using TxRxPacketStream = TxRxStreamBase<BVec, Eop>;

	/**
	 * @brief Connect tx to rx and tx to rx.
	 */
	template<Signal T = BVec, Signal... MetaT>
	Reverse<TxRxStreamBase<T, MetaT...>> crossover(TxRxStreamBase<T, MetaT...>&& in);
	inline auto crossover() { return [=](auto&& s1) { return crossover(std::forward<decltype(s1)>(s1)); }; }
}

namespace gtry::scl
{
	template<Signal T, Signal ...MetaT>
	Reverse<TxRxStreamBase<T, MetaT...>> crossover(TxRxStreamBase<T, MetaT...>&& in)
	{
		Reverse<TxRxStreamBase<T, MetaT...>> out = constructFrom(in);
		*out->rx <<= in.tx;
		*in.rx <<= out->tx;
		return out;
	}
}
