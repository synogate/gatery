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
#include "../io/TxRxStream.h"

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

	/**
	 * @brief Reduce data width by setting all unused data bits to zero and reducing the address width accordingly.
	 * @param dataWidthDivider New data path width is old width divided by dataWidthDivider. Needs to be power of 2 for the current implementation.
	 */
	TileLinkU tileLinkChopDataWidth(TileLinkU&& slave, size_t dataWidthDivider);
	inline auto tileLinkChopDataWidth(size_t dataWidthDivider) { return [=](auto&& in) { return tileLinkChopDataWidth(std::forward<decltype(in)>(in), dataWidthDivider); }; }

	/**
	 * @brief Serialize a TileLink over a packet stream. Can be used to use tile link in packet based or off chip scenarios.
	 *			This function is intended to be used between slave and packet stream. It does:
	 *				tx -> a
	 *				d -> rx
	 *			The function does some intelligent packing to reduce the transfer size on A and D channel:
	 *				- opcodes without data omit the data member
	 *				- bursts transfers send the header once, followed by a burst of data
	 *			Note that due to this optimization the data mask is not updated per beat. Thus partial put bursts are not supported.
	 * 
	 *			See driver::TileLinkSerializer to decode/encode the serialized format.
	 */
	Reverse<TxRxPacketStream> tileLinkSerialize(TileLinkU&& slave, BitWidth dataW);
	inline auto tileLinkSerialize(BitWidth dataW) { return [=](auto&& in) { return tileLinkSerialize(std::forward<decltype(in)>(in), dataW); }; }

	/**
	 * @brief The inverse of tileLinkSerialize.
	 *				a -> tx
	 *				rx -> d
	 */
	TileLinkU tileLinkDeserialize(Reverse<TxRxPacketStream>&& pipe, const TileLinkU& blueprint);
	inline auto tileLinkDeserialize(const TileLinkU& blueprint) { return [&](auto&& in) { return tileLinkDeserialize(std::forward<decltype(in)>(in), blueprint); }; }

	/**
	 * @brief Inserts a FiFo into the D channel path to buffer responses. It also stalls the A channel if the FiFo is too full to buffer more responses.
	 *			Can be used with devices that do not support back pressure on D channel like external memories.
	 * @param readLatencyLimit The device latency from request to response. Used to size the FiFo and configure the A channel stall condition.
	 */
	TileLinkU tileLinkFifoResultBuffer(TileLinkU&& slave, size_t readLatencyLimit, size_t minFifoDepth = 32);
	inline auto tileLinkFifoResultBuffer(size_t readLatencyLimit, size_t minFifoDepth = 32) { return [=](auto&& in) { return tileLinkFifoResultBuffer(std::forward<decltype(in)>(in), readLatencyLimit, minFifoDepth); }; }
}
