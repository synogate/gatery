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
	template<Signal T, StreamSignal StreamT>
	StreamT fifo(StreamT&& in, Fifo<T>& instance, FallThrough fallThrough = FallThrough::off);

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

	using gtry::connect;
	/**
	* @brief Connect a Stream as source to a FIFO as sink.
	* @param sink FIFO instance.
	* @param source Stream instance.
	*/
	template<Signal Tf, StreamSignal StreamT> 
	void connect(scl::Fifo<Tf>& sink, StreamT& source);
	template<Signal T, StreamSignal StreamT> requires std::is_assignable_v<T, typename std::remove_cvref_t<StreamT>::Payload>
	void connect(scl::Fifo<T>& sink, StreamT& source);


	/**
	* @brief Connect a FIFO as source to a Stream as sink.
	* @param sink Stream instance.
	* @param source FIFO instance.
	*/
	template<StreamSignal StreamT, Signal Tf> 
	void connect(StreamT& sink, scl::Fifo<Tf>& source);
	template<StreamSignal StreamT, Signal T> requires std::is_assignable_v<typename std::remove_cvref_t<StreamT>::Payload, T>
	void connect(StreamT& sink, scl::Fifo<T>& source);

	/**
	 * @brief Connect a Stream as source to a FIFO as sink. The different variants are used in case you want to save only the payload or also the additional MetaSignals.
	 * @param fifo The transactional FIFO
	 * @param inStream The source stream, from which the data is supposed to come.
	*/
	template<StreamSignal StreamTf, StreamSignal StreamT>
	void connect(scl::TransactionalFifo<StreamTf>& fifo, StreamT& inStream);
	template<Signal T, StreamSignal StreamT> requires std::is_assignable_v<T, typename std::remove_cvref_t<StreamT>::Payload>
	void connect(scl::TransactionalFifo<T>& fifo, StreamT& inStream);

	/**
	 * @brief Connect a FIFO as source to a stream as sink. The different variants are used in case you want to save only the payload or also the additional MetaSignals.
	 * @param packetStream The sink stream to which the data is supposed to flow.
	 * @param fifo The transactional FIFO
	*/
	template<StreamSignal StreamT>
	void connect(StreamT& packetStream, scl::TransactionalFifo<StreamData<StreamT>>& fifo);
	template<StreamSignal StreamT, StreamSignal StreamTf> requires std::is_assignable_v<typename std::remove_cvref_t<StreamT>::removeflowControl(), typename std::remove_cvref_t<StreamTf>::removeflowControl()>
	void connect(StreamT& packetStream, scl::TransactionalFifo<StreamTf>& fifo);


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

	template<Signal T, StreamSignal StreamT>
	StreamT fifo(StreamT&& in, Fifo<T>& instance, FallThrough fallThrough)
	{
		StreamT ret;
		connect(ret, instance);

		if (fallThrough == FallThrough::on) 
		{
			IF(!valid(ret))
			{
				downstream(ret) = downstream(in);
				IF(ready(ret))
					valid(in) = '0';
			}
		}
		connect(instance, in);

		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT fifo(StreamT&& in, size_t minDepth, FallThrough fallThrough)
	{
		Fifo inst{ minDepth, copy(downstream(in)) };
		StreamT ret = fifo(move(in), inst, fallThrough);
		inst.generate();

		return ret;
	}

	template<Signal Tf, StreamSignal StreamT> 
	void connect(scl::Fifo<Tf>& sink, StreamT& source)
	{
		IF(transfer(source))
			sink.push(downstream(source));
		ready(source) = !sink.full();
	}

	template<Signal T, StreamSignal StreamT> requires std::is_assignable_v<T, typename std::remove_cvref_t<StreamT>::Payload>
	void connect(scl::Fifo<T>& sink, StreamT& source)
	{
		IF(transfer(source))
			sink.push(*source);
		ready(source) = !sink.full();
	}

	template<StreamSignal StreamT, Signal Tf>
	void connect(StreamT& sink, scl::Fifo<Tf>& source)
	{
		downstream(sink) = source.peek();
		valid(sink) = !source.empty();

		IF(transfer(sink))
			source.pop();
	}

	template<StreamSignal StreamT, Signal T> requires std::is_assignable_v<typename std::remove_cvref_t<StreamT>::Payload, T>
	void connect(StreamT& sink, scl::Fifo<T>& source)
	{
		*sink = source.peek();
		valid(sink) = !source.empty();

		IF(transfer(sink))
			source.pop();
	}

	template<StreamSignal StreamTf, StreamSignal StreamT>
	void connect(scl::TransactionalFifo<StreamTf>& fifo, StreamT& inStream)
	{
		ready(inStream) = !fifo.full();

		IF(transfer(inStream))
		{
			fifo.push(inStream
				.template remove<Ready>()
				.template remove<Valid>()
				.template remove<Error>()
				.template remove<Sop>());

			IF(eop(inStream))
			{
				IF(error(inStream))
					fifo.rollbackPush();
				ELSE
					fifo.commitPush();
			}
		}
	}

	template<Signal T, StreamSignal StreamT> requires std::is_assignable_v<T, typename std::remove_cvref_t<StreamT>::Payload>
	void connect(scl::TransactionalFifo<T>& fifo, StreamT& inStream)
	{
		ready(inStream) = !fifo.full();

		IF(transfer(inStream))
		{
			fifo.push(*inStream);

			IF(eop(inStream))
			{
				IF(error(inStream))
					fifo.rollbackPush();
				ELSE
					fifo.commitPush();
			}
		}
	}

	template<StreamSignal StreamT>
	void connect(StreamT& packetStream, scl::TransactionalFifo<StreamData<StreamT>>& fifo)
	{
		*packetStream = *fifo.peek();

		// copy metadata
		std::apply([&](auto&&... meta) {
			((packetStream.template get<std::remove_cvref_t<decltype(meta)>>() = meta), ...);
			}, fifo.peek()._sig);

		if constexpr (packetStream.template has<Valid>())
			valid(packetStream) = !fifo.empty();

		if constexpr (packetStream.template has<Sop>())
			sop(packetStream) = !fifo.empty() & !flag(ready(packetStream) & !fifo.empty(), ready(packetStream) & eop(packetStream));

		IF(transfer(packetStream))
			fifo.pop();
	}

	template<StreamSignal StreamT, StreamSignal StreamTf> requires std::is_assignable_v<typename std::remove_cvref_t<StreamT>::removeflowControl(), typename std::remove_cvref_t<StreamTf>::removeflowControl()>
	void connect(StreamT& packetStream, scl::TransactionalFifo<StreamTf>& fifo)
	{
		*packetStream = fifo.peek();

		if constexpr (packetStream.template has<Valid>())
			valid(packetStream) = !fifo.empty();

		if constexpr (packetStream.template has<Sop>())
			sop(packetStream) = !fifo.empty() & !flag(ready(packetStream) & !fifo.empty(), ready(packetStream) & eop(packetStream));

		IF(transfer(packetStream))
			fifo.pop();
	}

	template<StreamSignal StreamT>
	auto storeForwardFifo(StreamT& in, size_t minElements)
	{
		TransactionalFifo fifo(minElements, in
			.template remove<Error>()
			.template remove<Sop>()
			.template remove<Ready>()
			.template remove<Valid>());

		connect(fifo, in);

		decltype(in	.template remove<Error>()
					.template remove<Sop>()
					.template add(Ready{})
					.template add(Valid{})
					.template add(Eop{})) out;
		connect(out, fifo);

		fifo.generate();
		return out;
	}
}

namespace gtry::scl
{
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
		auto ret = fifo	.peek()
						.add(Ready{})
						.add(Valid{ !fifo.empty() })
						.template reduceTo<RvStream<PayloadT, Meta...>>();

		IF(transfer(ret))
			fifo.pop();

		return ret;
	}
}
