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
#include "tilelink.h"

namespace gtry::scl
{
	class TileLinkStreamFetch
	{
	public:
		struct Command
		{
			UInt address;
			UInt beats;
		};

	public:
		TileLinkStreamFetch();

		TileLinkStreamFetch& pause(Bit condition) { m_pauseFetch = condition; return *this; }
		TileLinkStreamFetch& enableBursts(size_t maxBurstSizeInBits) { m_maxBurstSizeInBits = maxBurstSizeInBits; return *this; }

		//TODO: it supports multiple requests in parallel, but the responses are not guaranteed to be in order if the slave is not in order
		virtual TileLinkUB generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut, BitWidth sourceW = 0_b);

	private:
		Area m_area = {"scl_TileLinkStreamFetch", true};
		std::optional<Bit> m_pauseFetch;
		std::optional<size_t> m_maxBurstSizeInBits;
	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkStreamFetch::Command, address, beats);

namespace gtry 
{
	extern template void connect(scl::TileLinkStreamFetch::Command&, scl::TileLinkStreamFetch::Command&);
#if !defined(__clang__) || __clang_major__ >= 14
	extern template auto upstream(scl::TileLinkStreamFetch::Command&);
	extern template auto downstream(scl::TileLinkStreamFetch::Command&);
#endif
}