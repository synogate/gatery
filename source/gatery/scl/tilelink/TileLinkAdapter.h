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
#include "../stream/StreamArbiter.h"

namespace gtry::scl
{
	/**
	 * @brief Adapt between two tile link ports if possible by:
	 *			- reduce BitWidth where allowed
	 *			- AddBurst
	 *			- Double/HalfWidth
	 */
	void tileLinkAdapt(TileLinkU&& slave, TileLinkU&& master);
	inline auto tileLinkAdapt(TileLinkU&& master) { return [&](auto&& in) { return tileLinkAdapt(std::forward<decltype(in)>(in), master); }; }

	scl::TileLinkU tileLinkHalfWidth(scl::TileLinkU&& slave);
	inline auto tileLinkHalfWidth() { return [=](auto&& in) { return tileLinkHalfWidth(std::forward<decltype(in)>(in)); }; }

	scl::TileLinkU tileLinkDoubleWidth(scl::TileLinkU& slave);
	inline scl::TileLinkU tileLinkDoubleWidth(scl::TileLinkU&& slave) { return tileLinkDoubleWidth(slave); }
	inline auto tileLinkDoubleWidth() {	return [=](auto&& in) { return tileLinkDoubleWidth(std::forward<decltype(in)>(in)); }; }

	scl::TileLinkU tileLinkAddBurst(scl::TileLinkU&& slave, size_t burstLimitLog);
	scl::TileLinkU tileLinkAddBurst(scl::TileLinkU&& slave, BitWidth sizeW);

	inline auto tileLinkAddBurst(size_t burstLimitLog) { return [=](auto&& in) { return tileLinkAddBurst(std::forward<decltype(in)>(in), burstLimitLog); }; }
	inline auto tileLinkAddBurst(BitWidth sizeW) { return [=](auto&& in) { return tileLinkAddBurst(std::forward<decltype(in)>(in), sizeW); }; }

	/**
	 * @brief Makes a high clock rate slave accessible from a much slower master clock with single cycle latency.
	 * @param requestLaunchClk The master clock domain that generates the A stream.
	 * @param responseLatchClk The phase shifted master clock. Such that, a request is guaranteed to be finished between launch and latch edges.
	 * @param slaveClk
	 */
	TileLinkU tileLinkDynamicToFixedLatencyCdc(TileLinkU&& slave, const Clock& requestLaunchClk, const Clock& responseLatchClk, const Clock& slaveClk);
	inline auto tileLinkDynamicToFixedLatencyCdc(const Clock& requestLaunchClk, const Clock& responseLatchClk, const Clock& slaveClk) { return [=](auto&& in) { return tileLinkDynamicToFixedLatencyCdc(std::forward<decltype(in)>(in), requestLaunchClk, responseLatchClk, slaveClk); }; }

	/**
	 * @brief Stalls the request channel using stallCondition and strm::stall. The D channel is untouched.
	 */
	TileLinkU tileLinkStall(TileLinkU&& slave, Bit stallCondition);
	inline auto tileLinkStall(Bit stallCondition) { return [=](auto&& in) { return tileLinkStall(std::forward<decltype(in)>(in), stallCondition); }; }

}
