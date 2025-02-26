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
	namespace internal
	{
		struct RoCacheData
		{
			UInt tag;
			BVec value;
			Bit valid;
		};
	}

	/**
	 * @brief 
		- typical cache for the instruction fetch port of a CPU
		- direct mapped
		- read only. any non get access triggers undefined behavior.
		- the cpu interface does not support bursts
		- 8 byte bursts on memory interface iff address is aligned
		
	 * @param addrW Number of address bits for cache memory. The cache memory will have 2^addrW words.
	 * @param selectBurst Optional function to select the burst size and conditions. The function is called with the memory request channel and can modify the channel.
	 */
	TileLinkUL tileLinkCacheDirectMappedReadOnlyBurst(TileLinkUL&& slave, gtry::BitWidth addrW, std::function<void(TileLinkChannelA&)> selectBurst = {});
	inline auto tileLinkCacheDirectMappedReadOnlyBurst(BitWidth addrW, std::function<void(TileLinkChannelA&)> selectBurst = {}) { return [=](auto&& in) { return tileLinkAddBurst(std::forward<decltype(in)>(in), addrW, move(selectBurst)); }; }
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::internal::RoCacheData, tag, value, valid);
