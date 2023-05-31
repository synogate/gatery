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
#include "../Counter.h"

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
	const Bit sop(const T& signal) { return !flag(transfer(signal), transfer(signal) & eop(signal)); }


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
	const Bit valid(const T& signal) { return flag(sop(signal) & ready(signal), eop(signal) & ready(signal)) | sop(signal); }


	struct Empty
	{
		UInt empty; // number of empty symbols in this beat
	};

	template<StreamSignal T> requires (T::template has<Empty>())
	UInt& empty(T& s) { return s.template get<Empty>().empty; }
	template<StreamSignal T> requires (T::template has<Empty>())
	const UInt& empty(const T& s) { return s.template get<Empty>().empty; }

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

	template<Signal Payload, Signal... MetaIn>
	void connect(scl::TransactionalFifo<Payload>& fifo, Stream<Payload, Ready, MetaIn...>& inStream);

	template<Signal Payload, Signal... MetaFifo, Signal... MetaOut>
	void connect(Stream<Payload, MetaOut...>& packetStream, scl::TransactionalFifo<PacketStream<Payload, MetaFifo...>>& fifo);

	template<Signal Payload, Signal... MetaOut>
	void connect(Stream<Payload, MetaOut...>& packetStream, scl::TransactionalFifo<Payload>& fifo);

	template<Signal Payload, Signal... Meta> auto storeForwardFifo(RvPacketStream<Payload, Meta...>& in, size_t minElements);
	template<Signal Payload, Signal... Meta> auto storeForwardFifo(RsPacketStream<Payload, Meta...>& in, size_t minElements);

	template<StreamSignal T>
	requires (T::template has<Valid>() or T::template has<Eop>())
	T eraseLastBeat(T& source);

	template<StreamSignal T> 
	auto addEopDeferred(T& source, Bit insert);

	auto addPacketSignalsFromCount(StreamSignal auto& source, UInt size);

	template<Signal Payload, Signal ... Meta>
	RvPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const VPacketStream<Payload, Meta...>& source);

	template<Signal Payload, Signal ... Meta>
	RsPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const SPacketStream<Payload, Meta...>& source);

	template<BaseSignal Payload, Signal... Meta>
	auto streamShiftLeft(Stream<Payload, Meta...>& in, UInt shift, Bit reset = '0');

	UInt streamPacketBeatCounter(const StreamSignal auto& in, BitWidth counterW);

	template<BaseSignal Payload, Signal... Meta>
	UInt streamBeatBitLength(const Stream<Payload, Meta...>& in);

	// for internal use only, we should introduce a symbol width and remove this
	struct EmptyBits
	{
		UInt emptyBits;
	};

	template<StreamSignal Ts>
	const UInt emptyBits(const Ts& in) { return ConstUInt(0, BitWidth::count(in->width().bits())); }

	template<BaseSignal Payload, Signal... Meta> requires (not Stream<Payload, Meta...>::template has<EmptyBits>() and Stream<Payload, Meta...>::template has<Empty>())
	const UInt emptyBits(const Stream<Payload, Meta...>& in) { return cat(empty(in), "b000"); }

	template<BaseSignal Payload, Signal... Meta> requires (Stream<Payload, Meta...>::template has<EmptyBits>())
	UInt& emptyBits(Stream<Payload, Meta...>& in) { return in.template get<EmptyBits>().emptyBits; }

	template<BaseSignal Payload, Signal... Meta> requires (Stream<Payload, Meta...>::template has<EmptyBits>())
	const UInt& emptyBits(const Stream<Payload, Meta...>& in) { return in.template get<EmptyBits>().emptyBits; }
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::Eop, eop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Sop, sop);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Empty, empty);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::EmptyBits, emptyBits);

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

	template<Signal Payload, Signal... MetaIn>
	void connect(scl::TransactionalFifo<Payload>& fifo, Stream<Payload, Ready, MetaIn...>& inStream)
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

	template<Signal Payload, Signal... MetaOut>
	void connect(Stream<Payload, MetaOut...>& packetStream, scl::TransactionalFifo<Payload>& fifo)
	{
		*packetStream = fifo.peek();

		if constexpr (packetStream.template has<Valid>())
			valid(packetStream) = !fifo.empty();
		
		if constexpr (packetStream.template has<Sop>())
			sop(packetStream) = !fifo.empty() & !flag(ready(packetStream) & !fifo.empty(), ready(packetStream) & eop(packetStream));

		IF(transfer(packetStream))
			fifo.pop();
	}

	template<StreamSignal T>
	auto simuSop(const T& stream) {
		if constexpr (stream.template has<scl::Sop>())
			return simu(sop(stream));
		else
			return '1';
	}

	template<StreamSignal T>
	auto simuEop(const T& stream) {
		if constexpr (stream.template has<scl::Eop>())
			return simu(eop(stream));
		else
			return '1';
	}


	template<StreamSignal T>
	class SimuStreamPerformTransferWait {
	public:
		SimProcess wait(const T& stream, const Clock& clock) {
			if constexpr (stream.template has<Sop>()) {
				if (!m_isInPacket){
					do
						co_await internal::performTransferWait(stream, clock);
					while (!simu(sop(stream)));
					m_isInPacket = true;
				}
				else{
					co_await internal::performTransferWait(stream, clock);
					m_isInPacket = !(bool)simu(eop(stream));
				}
			}
			else {
				co_await internal::performTransferWait(stream, clock);
			}
		}

	protected:
		bool m_isInPacket = false;
	};

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

	namespace internal
	{
		template<Signal Payload, Signal ... Meta>
		auto addReadyAndFailOnBackpressure(const Stream<Payload, Meta...>& source)
		{
			Area ent{ "scl_addReadyAndFailOnBackpreasure", true };
			Stream ret = source
				.add(Ready{})
				.add(Error{ error(source) });

			Bit hadError = flag(valid(source) & !ready(ret), transfer(ret) & eop(ret));
			HCL_NAMED(hadError);
			error(ret) |= hadError;

			// If we have an EOP and we are not ready, we try to generate an eop.
			// If there is no bubble to generate the eop, we discard the entire next packet.
			Bit hadUnhandledEop = flag(valid(source) & eop(source), transfer(ret));
			HCL_NAMED(hadUnhandledEop);
			IF(hadUnhandledEop & !valid(source))
			{
				if constexpr (ret.template has<Valid>())
					valid(ret) = '1';
				eop(ret) = '1';
			}

			HCL_NAMED(ret);
			return ret;
		}
	}

	template<Signal Payload, Signal ... Meta>
	RvPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const VPacketStream<Payload, Meta...>& source)
	{
		return (RvPacketStream<Payload, Meta...>)internal::addReadyAndFailOnBackpressure(source);
	}

	template<Signal Payload, Signal ... Meta>
	RsPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const SPacketStream<Payload, Meta...>& source)
	{
		return (RsPacketStream<Payload, Meta...>)internal::addReadyAndFailOnBackpressure(source);
	}

	template<BaseSignal Payload, Signal... Meta>
	auto streamShiftLeft(Stream<Payload, Meta...>& in, UInt shift, Bit reset)
	{
		Area ent{ "scl_streamShiftLeft", true };
		HCL_DESIGNCHECK_HINT(shift.width() <= BitWidth::count(in->width().bits()), "beat shift not implemented");
		HCL_NAMED(shift);

		Stream out = in
			.template remove<scl::Empty>()
			.add(Eop{eop(in)})
			.add(EmptyBits{ emptyBits(in) });
		UInt& emptyBits = out.template get<EmptyBits>().emptyBits;

		Bit delayedEop;			HCL_NAMED(delayedEop);
		Bit shouldDelayEop = valid(in) & eop(in) & zext(emptyBits) < zext(shift);
		HCL_NAMED(shouldDelayEop);

		emptyBits -= resizeTo(shift, emptyBits.width());
		IF(shouldDelayEop & !delayedEop)
		{
			eop(out) = '0';
			ready(in) = '0';
		}

		ENIF(transfer(out))
		{
			Payload fullValue = (Payload)cat(*in, reg(*in));
			*out = (fullValue << shift).upper(out->width());
			HCL_NAMED(fullValue);

			delayedEop = flag(shouldDelayEop, delayedEop | reset);
		}
		HCL_NAMED(out);
		return out;
	}

	UInt streamPacketBeatCounter(const StreamSignal auto& in, BitWidth counterW)
	{
		scl::Counter counter{ counterW.count() };
		IF(transfer(in))
		{
			IF(!counter.isLast())
				counter.inc();
			IF(eop(in))
				counter.reset();
		}
		return counter.value();
	}

	template<BaseSignal Payload, Signal... Meta>
	UInt streamBeatBitLength(const Stream<Payload, Meta...>& in)
	{
		UInt len = in->width().bits();

		if constexpr (in.template has<scl::EmptyBits>())
		{
			IF(eop(in))
				len = in->width().bits() - zext(in.template get<scl::EmptyBits>().emptyBits);
		}
		else if constexpr (requires { empty(in); })
		{
			IF(eop(in))
			{
				UInt byteLen = in->width().bytes() - empty(in);
				len = cat(byteLen, "b000");
			}
		}
		return len;
	}
}
