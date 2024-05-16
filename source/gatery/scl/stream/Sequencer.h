/*
Synogate Library, a support library for Synogate IP-core generators.
Copyright (C) 2021 Synogate UG (haftungsbeschraenkt), www.synogate.com

This program is commercial software: you must not copy, modify or
distribute it without explicit written license by Synogate to do so.

The full license text for non-commercial use and evaluation can be found in the distribution package of this core.

This program is distributed WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY.
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
