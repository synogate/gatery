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
#include "Packet.h"
#include "../Counter.h"
#include "../Fifo.h"

namespace gtry::scl::strm
{
	using scl::performTransferWait;

	/**
	 * @brief extends the width of a simple payload stream. A 4-bit stream sent every beat extended to 8-bits means that the same data is send over an 8-bit bus once every 2 beats.
	 * @param source source stream
	 * @param width desired extended stream width
	 * @param reset line
	 * @return same typed stream as source input
	*/
	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>)
	auto extendWidth(T&& source, const BitWidth& width, Bit reset = '0');
	
	/**
	 * @brief reduces the width of a simple payload stream. An 8-bit stream reduced to 4 bits will be sent in double the amount of time.
	 * @param source source stream
	 * @param width desired reduced stream width
	 * @param reset line
	 * @return same typed stream as source input
	*/
	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload> and T::template has<Ready>())
	T reduceWidth(T&& source, BitWidth width, Bit reset = '0');

	/**
	 * @brief erases beat(s) from a multi-beat packet stream
	 * @param source source stream
	 * @param beatOffset when to start erasing beat(s)
	 * @param beatCount number of beats to erase
	 * @return same typed stream as source input
	*/
	template<StreamSignal T> 
	requires (T::template has<Ready>() and T::template has<Valid>())
	T eraseBeat(T&& source, UInt beatOffset, UInt beatCount);

	/**
	 * @brief inserts data into a multi-beat packet stream
	 * @param source source stream
	 * @param beatOffset which beat offset to start inserting data in
	 * @param value data to insert into stream
	 * @return same typed stream as source input
	*/
	template<StreamSignal T, SignalValue Tval> 
	requires (T::template has<Ready>())
	T insertBeat(T&& source, UInt beatOffset, const Tval& value);

	/**
	 * @brief stalls a stream using a stall condition. This function ensures a stream is backpressured without losing any data
	 * @param source source stream
	 * @param stallCondition when to stall (high -> stall)
	 * @return same typed stream as source input
	*/
	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T&& source, Bit stallCondition);

	/**
	 * @brief stalls a packet stream using a stall condition. This function ensures a stream is backpressured without losing any data. It will not interrupt an ongoing packet
	 * @param source source stream
	 * @param stallCondition when to stall (high -> stall)
	 * @return same typed stream as source input
	*/
	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stallPacket(T&& source, Bit stallCondition);

	/**
	 * @brief inserts a stream into a packet stream with bit offset precision.
	 * @param base stream into which data can be inserted
	 * @param insert data stream to insert into main packet stream
	 * @param bitOffset offset with which to start inserting data
	 * @return same typed stream as base input
	*/
	template<BaseSignal Payload, Signal ... Meta, Signal... MetaInsert>
	auto insert(RvPacketStream<Payload, Meta...>&& base, RvStream<Payload, MetaInsert...>&& insert, RvStream<UInt>&& bitOffset);

	/**
	 * @brief drops packets from a packet stream
	 * @param in input packet stream
	 * @param drop condition with which to drop packets. Must be asserted in the same cycle as the start of packet is transferred.
	 * @return same typed stream as in input
	*/
	template<scl::StreamSignal TStream>
	TStream dropPacket(TStream&& in, Bit drop);

	/**
	* @brief Puts a register in the ready, valid and data path.
	* @param stream Source stream
	* @param Settings forwarded to all instantiated registers.
	* @return connected stream
	*/
	template<StreamSignal T> 
	T regDecouple(T& stream, const RegisterSettings& settings = {});
	template<StreamSignal T>
	T regDecouple(const T& stream, const RegisterSettings& settings = {});

	/**
	* @brief Attach the stream as source and a new stream as sink to the FIFO.
	*			This is useful to make further settings or access advanced FIFO signals.
	*			For clock domain crossing you should use gtry::connect.
	* @param in The input stream.
	* @param instance The FIFO to use.
	* @param fallThrough allow data to flow past the fifo in the same cycle when it's empty.
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
	template<Signal Tf, StreamSignal Ts>
	void connect(scl::Fifo<Tf>& sink, Ts& source);

	template<Signal T>
	void connect(scl::Fifo<T>& sink, RvStream<T>& source);

	/**
	* @brief Connect a FIFO as source to a Stream as sink.
	* @param sink Stream instance.
	* @param source FIFO instance.
	*/
	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, scl::Fifo<Tf>& source);
	template<Signal T>
	void connect(RvStream<T>& sink, scl::Fifo<T>& source);

	/**
	 * @brief allows to send a request-acknowledge handshaked data across clock domains
	 * @param in input stream with data to pass across clock domains 
	 * @param inClock clock domain of input stream
	 * @param outClock clock domain of returned stream
	*/
	template<Signal Tp, Signal... Meta>
	RvStream<Tp, Meta...> synchronizeStreamReqAck(RvStream<Tp, Meta...>& in, const Clock& inClock, const Clock& outClock);
}

namespace gtry::scl::strm
{
	template<BaseSignal T>
	T makeShiftReg(BitWidth size, const T& in, const Bit& en)
	{
		T value = size;

		T newValue = value >> (int)in.width().bits();
		newValue.upper(in.width()) = in;

		IF(en)
			value = newValue;
		value = reg(value);
		return newValue;
	}

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>)
	auto extendWidth(T&& source, const BitWidth& width, Bit reset)
	{
		HCL_DESIGNCHECK(source->width() <= width);
		const size_t ratio = width / source->width();

		auto scope = Area{ "scl_extendWidth" }.enter();

		Counter counter{ ratio };
		IF(transfer(source))
			counter.inc();
		IF(reset)
			counter.reset();

		auto ret = source.add(
			Valid{ counter.isLast() & valid(source) }
		);
		if constexpr (T::template has<Ready>())
			ready(source) = ready(ret) | !counter.isLast();

		ret->resetNode();
		*ret = makeShiftReg(width, *source, transfer(source));

		if constexpr (T::template has<ByteEnable>())
		{
			auto& be = byteEnable(ret);
			BitWidth srcBeWidth = be.width();
			be.resetNode();
			be = makeShiftReg(srcBeWidth * ratio, byteEnable(source), transfer(source));
		}

		HCL_NAMED(ret);
		return ret;
	}

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>
		and T::template has<Ready>())
	T reduceWidth(T&& source, BitWidth width, Bit reset)
	{
		auto scope = Area{ "scl_reduceWidth" }.enter();
		T out;

		HCL_DESIGNCHECK(source->width() >= width);
		const size_t ratio = source->width() / width;

		Counter counter{ ratio };
		IF(transfer(out))
			counter.inc();
		IF(!valid(source) | reset)
			counter.reset();

		out <<= source;
		ready(source) &= counter.isLast();

		out->resetNode();
		*out = (*source)(zext(counter.value(), +width) * width.bits(), width);

		if constexpr (out.template has<ByteEnable>())
		{
			BVec& be = byteEnable(out);
			BitWidth w = be.width() / ratio;
			be.resetNode();
			be = byteEnable(source)(zext(counter.value(), +w) * w.bits(), w);
		}

		if constexpr (out.template has<Eop>())
			eop(out) &= counter.isLast();
		if constexpr (out.template has<Sop>())
			sop(out) &= counter.isFirst();

		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T> 
	requires (T::template has<Ready>() and T::template has<Valid>())
	T eraseBeat(T&& source, UInt beatOffset, UInt beatCount)
	{
		auto scope = Area{ "scl_eraseBeat" }.enter();

		BitWidth beatLimit = std::max(beatOffset.width(), beatCount.width()) + 1;
		Counter beatCounter{ beatLimit.count() };
		IF(transfer(source))
		{
			IF(beatCounter.value() < zext(beatOffset + beatCount))
				beatCounter.inc();
			IF(eop(source))
				beatCounter.reset();
		}

		T out;
		out <<= source;

		IF(beatCounter.value() >= zext(beatOffset) & 
			beatCounter.value() < zext(beatOffset + beatCount))
		{
			valid(out) = '0';
			ready(source) = '1';
		}
		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T, SignalValue Tval> 
	requires (T::template has<Ready>())
	T insertBeat(T&& source, UInt beatOffset, const Tval& value)
	{
		auto scope = Area{ "scl_insertBeat" }.enter();
		T out;
		out <<= source;

		Counter beatCounter{ (beatOffset.width() + 1).count() };
		IF(transfer(out))
		{
			IF(beatCounter.value() < zext(beatOffset + 1))
				beatCounter.inc();
			IF(eop(source) & beatCounter.value() != zext(beatOffset))
				beatCounter.reset();
		}

		IF(beatCounter.value() == zext(beatOffset))
		{
			*out = value;
			ready(source) = '0';
			eop(out) = '0';
		}
		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T&& source, Bit stallCondition)
	{
		T out;
		out <<= source;
	
		IF(stallCondition)
		{
			valid(out) = '0';
			ready(source) = '0';
		}
		return out;
	}

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stallPacket(T&& source, Bit stallCondition)
	{
		return stall(move(source), stallCondition & sop(source));
	}

	template<BaseSignal Payload, Signal ... Meta, Signal... MetaInsert>
	auto insert(RvPacketStream<Payload, Meta...>&& base, RvStream<Payload, MetaInsert...>&& insert, RvStream<UInt>&& bitOffset)
	{
		Area ent{ "scl_streamInsert", true };
		HCL_DESIGNCHECK_HINT(base->width() == insert->width(), "insert width must match base width");

		UInt insertBitOffset = bitOffset->lower(BitWidth::count(base->width().bits()));	HCL_NAMED(insertBitOffset);
		UInt insertBeat = bitOffset->upper(-insertBitOffset.width());					HCL_NAMED(insertBeat);

		Bit baseShiftReset = !valid(bitOffset);											HCL_NAMED(baseShiftReset);
		UInt baseShift = insertBitOffset.width();										HCL_NAMED(baseShift);
		RvPacketStream baseShifted = streamShiftLeft(base, baseShift, baseShiftReset);	HCL_NAMED(baseShifted);
		RvPacketStream insertShifted = streamShiftLeft(insert, insertBitOffset);		HCL_NAMED(insertShifted);
		Bit insertShiftedShouldDelayEop = valid(insert) & eop(insert) & zext(emptyBits(insert)) < zext(insertBitOffset); HCL_NAMED(insertShiftedShouldDelayEop);

		RvPacketStream out = constructFrom(baseShifted);
		UInt beatCounter = streamPacketBeatCounter(out, insertBeat.width());			HCL_NAMED(beatCounter);

		IF(transfer(out) & eop(out))
			baseShift = 0;
		baseShift = reg(baseShift, 0);
		IF(valid(insert) & eop(insert))
			baseShift = streamBeatBitLength(insert).lower(-1_b);

		UInt emptyBitsInsert = capture(emptyBits(insert), valid(insert) & eop(insert));	HCL_NAMED(emptyBitsInsert);
		UInt emptyBitsBase = capture(emptyBits(base), valid(base) & eop(base));			HCL_NAMED(emptyBitsBase);
		UInt emptyBitsOut = emptyBitsInsert + emptyBitsBase;							HCL_NAMED(emptyBitsOut);

		downstream(out) = downstream(baseShifted);
		emptyBits(out) = emptyBitsOut;
		valid(out) = '0';
		ready(baseShifted) = '0';
		ready(insertShifted) = '0';

		enum class State
		{
			prefix,
			insert,
			suffix
		};
		Reg<Enum<State>> state{ State::prefix };
		state.setName("state");

		IF(state.current() == State::prefix)
		{
			ready(baseShifted) = ready(out);
			valid(out) = valid(base);
			*out = *base;

			IF(valid(bitOffset) & beatCounter == insertBeat)
				state = State::insert;
		}

		Bit sawEop; HCL_NAMED(sawEop);
		IF(state.combinatorial() == State::insert)
		{
			ready(baseShifted) = '0';
			ready(insertShifted) = ready(out);
			valid(out) = valid(insertShifted);
			*out = *insertShifted;

			IF(beatCounter == insertBeat)
			{
				for (size_t i = 0; i < out->width().bits(); i++)
					IF(i < insertBitOffset)
					(*out)[i] = (*base)[i];
			}

			UInt insertShift = zext(base->width().bits(), +1_b) - zext(emptyBits(insertShifted)) - zext(insertBitOffset);
			HCL_NAMED(insertShift);
			IF(valid(insert) & eop(insert) & insertShift.msb())
				ready(baseShifted) = ready(out);

			IF(eop(insertShifted))
			{
				UInt numBaseBits = emptyBits(insertShifted);
				HCL_NAMED(numBaseBits);

				for (size_t i = 1; i < out->width().bits(); i++)
					IF(out->width().bits() - i <= numBaseBits)
					(*out)[i] = (*baseShifted)[i];

				IF(!(numBaseBits == 0 & insertBitOffset == 0))
					ready(baseShifted) = ready(out);

				IF(valid(insertShifted) & ready(out))
					state = State::suffix;
			}
		}

		IF(state.current() == State::suffix)
		{
			ready(baseShifted) = ready(out);
			valid(out) = valid(baseShifted);
			*out = *baseShifted;
		}

		eop(out) = '0';
		IF(state.combinatorial() == State::suffix & sawEop)
		{
			eop(out) = '1';
			IF(transfer(out))
				state = State::prefix;
		}
		IF(valid(base) & eop(base) & !valid(bitOffset))
		{
			eop(out) = '1';
			emptyBits(out) = emptyBitsBase;
		}

		sawEop = flagInstantSet(transfer(baseShifted) & eop(baseShifted), transfer(out) & eop(out));

		ready(bitOffset) = valid(out) & eop(out);
		return out;
	}

	template<scl::StreamSignal TStream>
	TStream dropPacket(TStream&& in, Bit drop)
	{
		Area area("scl_streamDropPacket", true);
		HCL_NAMED(in);
		HCL_NAMED(drop);

		TStream out;
		out <<= in;
		Bit dropPacket = scl::flagInstantSet(drop & sop(in) & transfer(in), eop(in) & transfer(in));
		HCL_NAMED(dropPacket);

		if constexpr (TStream::template has<scl::Valid>())
			valid(out) &= !dropPacket;

		if constexpr (TStream::template has<scl::Sop>())
			sop(out) &= !dropPacket;

		if constexpr (TStream::template has<scl::Eop>())
			eop(out) &= !dropPacket;

		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T>
	T regDecouple(T& stream, const RegisterSettings& settings)
	{
		// we can use blocking reg here since regReady guarantees high ready signal
		return strm::regReady(strm::regDownstreamBlocking(move(stream), settings), settings);
	}

	template<StreamSignal T>
	T regDecouple(const T& stream, const RegisterSettings& settings)
	{
		static_assert(!stream.template has<Ready>(), "cannot create upstream register from const stream");
		return strm::regDownstream(stream, settings);
	}

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
		Stream ret = fifo(move(in), inst, fallThrough);
		inst.generate();

		return ret;
	}

	template<Signal Tf, StreamSignal Ts>
	void connect(scl::Fifo<Tf>& sink, Ts& source)
	{
		IF(transfer(source))
			sink.push(downstream(source));
		ready(source) = !sink.full();
	}

	template<Signal T>
	void connect(scl::Fifo<T>& sink, RvStream<T>&source)
	{
		IF(transfer(source))
			sink.push(*source);
		ready(source) = !sink.full();
	}

	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, scl::Fifo<Tf>& source)
	{
		downstream(sink) = source.peek();
		valid(sink) = !source.empty();

		IF(transfer(sink))
			source.pop();
	}

	template<Signal T>
	void connect(RvStream<T>& sink, scl::Fifo<T>& source)
	{
		*sink = source.peek();
		valid(sink) = !source.empty();

		IF(transfer(sink))
			source.pop();
	}

	template<Signal Tp, Signal... Meta>
	RvStream<Tp, Meta...> synchronizeStreamReqAck(RvStream<Tp, Meta...>& in, const Clock& inClock, const Clock& outClock)
	{
		Area area("synchronizeStreamReqAck", true);
		ClockScope csIn{ inClock };
		Stream crossingStream = in
			.template remove<Ready>()
			.template remove<Valid>();

		Bit eventIn;
		Bit idle = flag(ready(in), eventIn, '1');
		eventIn = valid(in) & idle;
		HCL_NAMED(eventIn);	

		Bit outputEnableCondition = synchronizeEvent(eventIn, inClock, outClock);
		HCL_NAMED(outputEnableCondition);

		crossingStream = reg(crossingStream);

		ClockScope csOut{ outClock };

		crossingStream = allowClockDomainCrossing(crossingStream, inClock, outClock);

		ENIF(outputEnableCondition){
			Clock dontSimplifyEnableRegClk = outClock.deriveClock({ .synchronizationRegister = true });
			crossingStream = reg(crossingStream, RegisterSettings{ .clock = dontSimplifyEnableRegClk });
		}

		RvStream out = crossingStream
			.add(Ready{})
			.add(Valid{})
			.template reduceTo<RvStream<Tp, Meta...>>();

		Bit outValid;
		outValid = flag(outputEnableCondition, outValid & ready(out));
		valid(out) = outValid;

		ready(in) = synchronizeEvent(transfer(out), outClock, inClock);

		return out;
	}

}
