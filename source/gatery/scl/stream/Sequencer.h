/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include "Stream.h"

namespace gtry::scl::strm
{
	/**
	 * @brief takes an out of order txid stream and reorders it.
	 * @param
	 * @return the output type is an RvStream, regardless of whether the input is an RvStream or a VStream
	*/
	template<StreamSignal StreamT> requires (StreamT::template has<TxId>())
	auto sequencer(StreamT&& input);

	/**
	 * @brief pipeable overload : auto orderedStream = move(streamToReorder) | synogate::sequencer();
	*/
	inline auto sequencer()
	{
		return [=](auto&& source) { return sequencer(std::forward<decltype(source)>(source)); };
	}
}


namespace gtry::scl::strm
{
	template<StreamSignal StreamT> requires (StreamT::template has<TxId>())
	auto sequencer(StreamT&& input) 
	{
		Area area{ "scl_sequencer", true };
		HCL_NAMED(input);

		struct Validity {
			Bit validity;
		};
		auto inputWithValidity = move(input).add(Validity{});

		Memory reorderBuffer(txid(input).width().count(), inputWithValidity.removeFlowControl().template remove<TxId>());
		reorderBuffer.initZero();
		// we synchronize through memory content, so we dont need to add latency to the write port during retiming
		reorderBuffer.allowArbitraryPortRetiming();

		//output logic
		Counter orderCtr(txid(input).width() + 1_b); //add an extra bit to signal current validity
		UInt currentTxid = orderCtr.value().lower(-1_b);
		Bit outputValidityPolarity = !orderCtr.value().msb();

		Stream memoryElement = reorderBuffer[currentTxid].read();
		for(size_t i = 0; i < reorderBuffer.readLatencyHint(); i++)
			memoryElement = pipestage(memoryElement);

		auto outputStream = move(memoryElement)
			.add(Ready{})
			.add(Valid{ outputValidityPolarity == memoryElement.template get<Validity>().validity })
			.add(TxId{ currentTxid })
			.template remove<Validity>();
		HCL_NAMED(outputStream);

		IF(transfer(outputStream))
			orderCtr.inc();
		
		//input logic
		inputWithValidity.template get<Validity>().validity = outputValidityPolarity ^ (txid(inputWithValidity) < currentTxid);
		if constexpr (StreamT::template has<Ready>())
			ready(inputWithValidity) = '1';

		IF(transfer(inputWithValidity))
			reorderBuffer[txid(inputWithValidity)] = move(inputWithValidity).removeFlowControl().template remove<TxId>();

		return move(outputStream) | scl::strm::regDownstream();
	};
}
