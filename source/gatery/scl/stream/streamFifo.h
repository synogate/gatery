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

#include "StreamConcept.h" 
#include "../Fifo.h"
#include "../TransactionalFifo.h"


namespace gtry::scl::strm {
	/**
	* @brief Attach the stream as source and a new stream as sink to the FIFO.
	*			This is useful to make further settings or access advanced FIFO signals.
	*			For clock domain crossing you should use gtry::connect.
	* @param in The input stream.
	* @param instance The FIFO to use.
	* @param fallThrough allow data to flow past the fifo in the same cycle when it's empty. See Fallthrough struct
	* @return connected stream
	*/
	//template<Signal T, StreamSignal StreamT>
	//StreamT fifo(StreamT&& in, Fifo<T>& instance, FallThrough fallThrough = FallThrough::off);

	/**
	* @brief Create a FIFO for buffering.
	* @param in The input stream.
	* @param minDepth The FIFO can hold at least that many data beats.
	The actual amount depends on the available target architecture.
	* @param fallThrough allow data to flow past the fifo in the same cycle when it's empty.
	* @return connected stream
	*/
	template<StreamSignal StreamT>
	StreamT fifo(StreamT&& in, size_t minDepth = 16, FallThrough fallThrough = FallThrough::off);

	inline auto fifo(size_t minDepth = 16, FallThrough fallThrough = FallThrough::off)
	{
		return [=](auto&& in) { return fifo(std::forward<decltype(in)>(in), minDepth, fallThrough); };
	}

	/**
	 * @brief Returns a stream that is connected to the pop port of the FIFO.
	 *		The stream is valid when the FIFO is not empty and contains the peek data from the FIFO.
	*/
	template<Signal PayloadT>
	RvStream<PayloadT> pop(Fifo<PayloadT>& fifo);
	template<Signal PayloadT, Signal... Meta>
	RvStream<PayloadT, Meta...> pop(Fifo<Stream<PayloadT, Meta...>>& fifo);

	/**
	 * @brief Connects the in stream to the push port of the FIFO. Ready of in is driven by no full.
	*/
	template<StreamSignal StreamT>
	void push(Fifo<typename StreamT::Payload>& fifo, StreamT&& in);
	template<StreamSignal StreamT>
	void push(Fifo<StreamData<StreamT>>& fifo, StreamT&& in);

	template<StreamSignal T>
	using StreamDataNoError = decltype(std::declval<T>().removeFlowControl().template remove<Error>());

	/**
	 * @brief Same as push but packets are stored in full before calling rollback or commit based on error of in.
	*/
	template<StreamSignal StreamT>
	void pushStoreForward(TransactionalFifo<typename StreamT::Payload>& fifo, StreamT&& in);
	template<StreamSignal StreamT>
	void pushStoreForward(TransactionalFifo<StreamDataNoError<StreamT>>& fifo, StreamT&& in);

	/**
	 * @brief puts a store forward fifo in the stream and returns a Ready-Valid-Eop Stream. For information on store-forward FIFOS, 
	 * see: https://www.synopsys.com/dw/dwtb/dwc_fifo_size/dwc_fifo_size.html#:~:text=%22Store%20and%20Forward%22%20works%20as,system%20or%20a%20DMA%20controller.
	 * @param in Input Stream.
	 * @param minElements minimum amount of elements needed to be stored in the FIFO
	 * @return 
	*/
	template<StreamSignal StreamT> auto storeForwardFifo(StreamT& in, size_t minElements);
}


namespace gtry::scl::strm {

	template<StreamSignal StreamT>
	StreamT fifo(StreamT&& in, Fifo<StreamData<StreamT>>& instance, FallThrough fallThrough)
	{
		StreamT ret = pop(instance);

		if (fallThrough == FallThrough::on) 
		{
			IF(!valid(ret))
			{
				downstream(ret) = downstream(in);
				IF(ready(ret))
					valid(in) = '0';
			}
		}

		ready(in) = !instance.full();
		IF(transfer(in))
			instance.push(in.removeFlowControl());

		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT fifo(StreamT&& in, size_t minDepth, FallThrough fallThrough)
	{
		Fifo<StreamData<StreamT>> inst{ minDepth, in.removeFlowControl() };
		StreamT ret = fifo(move(in), inst, fallThrough);
		inst.generate();

		return ret;
	}

	template<StreamSignal StreamT>
	auto storeForwardFifo(StreamT& in, size_t minElements)
	{
		TransactionalFifo fifo(minElements, in
			.removeFlowControl()
			.template remove<Error>()
			.template remove<Sop>());

		pushStoreForward(fifo, in);

		RvStream out = pop(fifo);

		fifo.generate();
		return out;
	}

	template<Signal PayloadT>
	RvStream<PayloadT> pop(Fifo<PayloadT>& fifo)
	{
		RvStream<PayloadT> ret = { fifo.peek() };
		valid(ret) = !fifo.empty();
		IF(transfer(ret))
			fifo.pop();
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	RvStream<PayloadT, Meta...> pop(Fifo<Stream<PayloadT, Meta...>>& fifo)
	{
		auto ret = fifo.peek()
			.add(Ready{})
			.add(Valid{ !fifo.empty() })
			.template reduceTo<RvStream<PayloadT, Meta...>>();

		IF(transfer(ret))
			fifo.pop();

		return ret;
	}

	template<StreamSignal StreamT>
	void push(Fifo<typename StreamT::Payload>& fifo, StreamT&& in)
	{
		ready(in) = !fifo.full();
		IF(transfer(in))
			fifo.push(*in);
	}

	template<StreamSignal StreamT>
	void push(Fifo<StreamData<StreamT>>& fifo, StreamT&& in)
	{
		ready(in) = !fifo.full();
		IF(transfer(in))
			fifo.push(in.removeFlowControl());
	}

	template<Signal PayloadT, StreamSignal StreamT>
	void commitOrRollbackOnEop(TransactionalFifo<PayloadT>& fifo, const StreamT& in)
	{
		IF(transfer(in) & eop(in))
		{
			IF(error(in))
				fifo.rollbackPush();
			ELSE
				fifo.commitPush();
		}
	}

	template<StreamSignal StreamT>
	void pushStoreForward(TransactionalFifo<typename StreamT::Payload>& fifo, StreamT&& in)
	{
		commitOrRollbackOnEop(fifo, in);
		push(fifo, move(in));
	}

	template<StreamSignal StreamT>
	void pushStoreForward(TransactionalFifo<StreamDataNoError<StreamT>>& fifo, StreamT&& in)
	{
		commitOrRollbackOnEop(fifo, in);
		push(fifo, move(in).template remove<Error>());
	}
}
