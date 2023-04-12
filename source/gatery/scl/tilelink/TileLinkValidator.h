/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
	SimProcess validateTileLink(TileLinkChannelA &channelA, TileLinkChannelD &channelD, const Clock &clk);
	SimProcess validateTileLinkControlSignalsDefined(TileLinkChannelA& a, const Clock& clk);
	SimProcess validateTileLinkControlSignalsDefined(TileLinkChannelD& a, const Clock& clk);
	SimProcess validateTileLinkSourceReuse(TileLinkChannelA& channelA, TileLinkChannelD& channelD, const Clock& clk);
	SimProcess validateTileLinkResponseMatchesRequest(TileLinkChannelA& channelA, TileLinkChannelD& channelD, const Clock& clk);
	SimProcess validateTileLinkNoBurst(TileLinkChannelA& a, const Clock& clk);
	SimProcess validateTileLinkBurst(TileLinkChannelA& a, const Clock& clk);
	SimProcess validateTileLinkBurst(TileLinkChannelD& d, const Clock& clk);
	SimProcess validateTileLinkAlignment(TileLinkChannelA& a, const Clock& clk);
	SimProcess validateTileLinkOperations(TileLinkChannelA& a, std::vector<TileLinkA::OpCode> whitelist, const Clock& clk);
	SimProcess validateTileLinkMask(TileLinkChannelA& a, const Clock& clk);

	template<TileLinkSignal TileLinkType>
	SimProcess validate(TileLinkType& tileLink, const Clock& clk);
}

namespace gtry::scl::internal
{
	template<TileLinkSignal TLink> 
	std::vector<TileLinkA::OpCode> tileLinkValidOps(const TLink& link)
	{
		std::vector<TileLinkA::OpCode> ret = {
			TileLinkA::Get,
			TileLinkA::PutFullData,
			TileLinkA::PutPartialData
		};
		
		if constexpr (TLink::template capability<TileLinkCapHint>())
			ret.push_back(TileLinkA::Intent);

		if constexpr (TLink::template capability<TileLinkCapAtomicArith>())
			ret.push_back(TileLinkA::ArithmeticData);

		if constexpr (TLink::template capability<TileLinkCapAtomicLogic>())
			ret.push_back(TileLinkA::LogicalData);

		return ret;
	}
}

namespace gtry::scl
{
	template<TileLinkSignal TileLinkType>
	SimProcess validate(TileLinkType& tileLink, const Clock& clk)
	{
		fork(validateTileLink(tileLink.a, *tileLink.d, clk));

		if constexpr (!TileLinkType::template capability<TileLinkCapBurst>())
			fork(validateTileLinkNoBurst(tileLink.a, clk));

		auto ops = internal::tileLinkValidOps(tileLink);
		fork(validateTileLinkOperations(tileLink.a, ops, clk));
		co_return;
	}

	template<TileLinkSignal TileLinkType>
	SimProcess validateTileLinkMemory(TileLinkType& link, const Clock& clk)
	{
		return validateTileLinkMemory(link.a, *link.d, clk);
	}
}
