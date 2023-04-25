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
#include "Stream.h"
#include "../TransactionalFifo.h"

namespace gtry::scl
{
	struct Eop
	{
		Bit eop;
	};

	template<StreamSignal T> requires (T::template has<Eop>())
	Bit& eop(T& stream) { return stream.template get<Eop>().eop; }
	template<StreamSignal T> requires (T::template has<Eop>())
	const Bit& eop(const T& stream) { return stream.template get<Eop>().eop; }
	template<StreamSignal T> requires (T::template has<Valid>() and T::template has<Eop>())
	const Bit sop(const T& signal) { return !flag(transfer(signal), eop(signal)); }


	struct Sop
	{
		// reset to zero, sop is used for packet streams without valid.
		Bit sop = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal T> requires (T::template has<Sop>())
	Bit& sop(T& stream) { return stream.template get<Sop>().sop; }
	template<StreamSignal T> requires (T::template has<Sop>())
	const Bit& sop(const T& stream) { return stream.template get<Sop>().sop; }
	template<StreamSignal T> requires (!T::template has<Valid>() and T::template has<Sop>() and T::template has<Eop>())
	const Bit valid(const T& signal) { return flag(sop(signal), eop(signal) & ready(signal)) | sop(signal); }


	struct Empty
	{
		UInt empty; // number of empty symbols in this beat
	};

	UInt& empty(StreamSignal auto& s) { return s.template get<Empty>().empty; }
	const UInt& empty(const StreamSignal auto& s) { return s.template get<Empty>().empty; }

	template<Signal T, Signal... Meta>
	using PacketStream = Stream<T, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using RvPacketStream = Stream<T, scl::Ready, scl::Valid, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using VPacketStream = Stream<T, scl::Valid, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using RsPacketStream = Stream<T, scl::Ready, scl::Sop, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using SPacketStream = Stream<T, scl::Sop, scl::Eop, Meta...>;

	template<class T>
	concept PacketStreamSignal = StreamSignal<T> and T::template has<scl::Eop>();

	template<Signal Payload, Signal... MetaIn, Signal... MetaFifo>
	void connect(scl::TransactionalFifo<PacketStream<Payload, MetaFifo...>>& fifo, Stream<Payload, Ready, MetaIn...>& inStream);

	template<Signal Payload, Signal... MetaFifo, Signal... MetaOut>
	void connect(Stream<Payload, MetaOut...>& packetStream, scl::TransactionalFifo<PacketStream<Payload, MetaFifo...>>& fifo);

	template<Signal Payload, Signal... Meta> auto storeForwardFifo(RvPacketStream<Payload, Meta...>& in, size_t minElements);
	template<Signal Payload, Signal... Meta> auto storeForwardFifo(RsPacketStream<Payload, Meta...>& in, size_t minElements);

	template<StreamSignal T>
	requires (T::template has<Valid>() or T::template has<Eop>())
	T eraseLastBeat(T& source);

	template<StreamSignal T> 
	auto addEopDeferred(T& source, Bit insert);

	auto addPacketSignalsFromCount(StreamSignal auto& source, UInt size);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::Eop, eop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Sop, sop);

namespace gtry::scl
{
	template<Signal Payload, Signal... MetaIn, Signal... MetaFifo>
	void connect(scl::TransactionalFifo<PacketStream<Payload, MetaFifo...>>& fifo, Stream<Payload, Ready, MetaIn...>& inStream)
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

	template<Signal Payload, Signal... MetaFifo, Signal... MetaOut>
	void connect(Stream<Payload, MetaOut...>& packetStream, scl::TransactionalFifo<PacketStream<Payload, MetaFifo...>>& fifo)
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

	template<Signal Payload, Signal... Meta>
	auto storeForwardFifo(RvPacketStream<Payload, Meta...>& in, size_t minElements)
	{
		TransactionalFifo fifo(minElements, in
			.template remove<Error>()
			.template remove<Sop>()
			.template remove<Ready>()
			.template remove<Valid>());

		connect(fifo, in);

		decltype(in.template remove<Error>().template remove<Sop>()) out;
		connect(out, fifo);

		fifo.generate();
		return out;
	}

	template<Signal Payload, Signal... Meta>
	auto storeForwardFifo(RsPacketStream<Payload, Meta...>& in, size_t minElements)
	{
		TransactionalFifo fifo(minElements, in.template remove<Valid>().template remove<Error>().template remove<Ready>().template remove<Sop>());
		connect(fifo, in);

		decltype(in.template remove<Valid>().template remove<Error>()) out;
		connect(out, fifo);

		fifo.generate();
		return out;
	}

	template<StreamSignal T>
		requires (T::template has<Valid>() or T::template has<Eop>())
	T eraseLastBeat(T& source)
	{
		auto scope = Area{ "scl_eraseLastBeat" }.enter();
		T in;
		in <<= source;
		HCL_NAMED(in);

		if constexpr (T::template has<Valid>())
			IF(eop(source))
			valid(in) = '0';

		T out = constructFrom(in);
		out = in.regDownstream();

		if constexpr (T::template has<Eop>())
		{
			Bit eopReg = flag(eop(source) & valid(source), transfer(out));
			IF(eop(source) | eopReg)
				eop(out) = '1';
		}
		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T>
	auto addEopDeferred(T& source, Bit insert)
	{
		auto scope = Area{ "scl_addEopDeferred" }.enter();

		auto in = source.add(scl::Eop{ '0' });
		HCL_NAMED(in);

		Bit insertState;
		HCL_NAMED(insertState);
		IF(insertState)
		{
			ready(source) = '0';
			valid(in) = '1';
			eop(in) = '1';
		}

		auto out = scl::eraseLastBeat(in);
		insertState = flag(insert, transfer(out)) | insert;
		HCL_NAMED(out);
		return out;
	}

	auto addPacketSignalsFromCount(StreamSignal auto& source, UInt size)
	{
		auto scope = Area{ "scl_addPacketSignalsFromSize" }.enter();

		UInt beatCounter = size.width();
		UInt beatCounterNext = beatCounter + 1;

		Bit start;
		IF(transfer(source))
		{
			//sim_assert(size != 0) << "what is a zero length packet?";
			start = '0';
			beatCounter = beatCounterNext;
		}

		Bit end = '0';
		IF(beatCounterNext == size)
		{
			end = '1';
			IF(transfer(source))
			{
				start = '1';
				beatCounter = 0;
			}
		}
		start = reg(start, '1');
		beatCounter = reg(beatCounter, 0);

		HCL_NAMED(beatCounter);
		auto out = source.add(Eop{ end }).add(Sop{ start });
		return out;
	}
}
