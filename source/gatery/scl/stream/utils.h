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

#include "../Counter.h"
#include "metaSignals.h"
#include "../cdc.h"

namespace gtry::scl::strm
{

	/**
	* @brief	Puts a register in the valid and data path.
	The register enable is connected to ready and ready is just forwarded.
	Note that valid will not become high while ready is low, which violates stream semantics.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regDownstreamBlocking(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	inline auto regDownstreamBlocking(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDownstreamBlocking(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief	Puts a register in the ready path and adds additional circuitry to keep the expected behavior of a stream
	* @param	in The input stream.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regReady(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	inline auto regReady(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regReady(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief	Puts a register in the valid and data path.
	This version ensures that data is captured when ready is low to fill pipeline bubbles.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regDownstream(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	inline auto regDownstream(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDownstream(std::forward<decltype(in)>(in), settings); };
	}

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
	* @brief allows to send a request-acknowledge handshaked data across clock domains
	* @param in input stream with data to pass across clock domains 
	* @param inClock clock domain of input stream
	* @param outClock clock domain of returned stream
	*/
	template<StreamSignal StreamT>
	StreamT synchronizeStreamReqAck(StreamT& in, const Clock& inClock, const Clock& outClock);

	/**
	* @brief attaches a memory to a stream carrying addresses and returns a stream of the memory data
	* @param addr the address stream to look up in memory
	* @param memory - self explanatory
	*/
	template<Signal T, Signal ... Meta, StreamSignal StreamT>
	StreamT lookup(StreamT& addr, Memory<T>& memory);

	/**
	* @brief	Puts a register spawner for retiming in the valid and data path.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group);

	//untested
	inline auto pipeinputDownstream(PipeBalanceGroup& group)
	{
		return [=](auto&& in) { return pipeinputDownstream(std::forward<decltype(in)>(in), group); };
	}

	using gtry::pipeinput;
	/// Add register spawners to the downstream signals which are enabled by the upstream ready (if it exists).
	template<StreamSignal T>
	T pipeinput(T&& in)
	{
		T out;
		ENIF(ready(out))
		{
			PipeBalanceGroup group;
			if constexpr (T::template has<Valid>())
				valid(in).resetValue('0');

			downstream(out) = gtry::pipeinput(copy(downstream(in)), group);
		}
		upstream(in) = upstream(out);
		return out;
	}
}

namespace gtry::scl::strm
{
	template<StreamSignal StreamT>
	inline StreamT regDownstreamBlocking(StreamT&& in, const RegisterSettings& settings)
	{
		if constexpr (in.template has<Valid>())
			valid(in).resetValue('0');

		auto dsSig = constructFrom(copy(downstream(in)));

		IF(ready(in))
			dsSig = downstream(in);

		dsSig = reg(dsSig, settings);

		StreamT ret;
		downstream(ret) = dsSig;
		upstream(in) = upstream(ret);
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT regReady(StreamT &&in, const RegisterSettings& settings)
	{
		if constexpr (in.template has<Valid>())
			valid(in).resetValue('0');
		if constexpr (in.template has<Ready>())
			ready(in).resetValue('0');

		StreamT ret = constructFrom(in);
		ret <<= in;

		if constexpr (in.template has<Ready>())
		{
			Bit valid_reg;
			auto data_reg = constructFrom(copy(downstream(in)));

			// we are ready as long as our buffer is unused
			ready(in) = !valid_reg;

			IF(ready(ret))
				valid_reg = '0';

			IF(!valid_reg)
			{
				IF(!ready(ret))
					valid_reg = valid(in);
				data_reg = downstream(in);
			}

			valid_reg = reg(valid_reg, '0', settings);
			data_reg = reg(data_reg, settings);

			IF(valid_reg)
			{
				downstream(ret) = data_reg;
				valid(ret) = '1';
			}
		}
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT regDownstream(StreamT &&in, const RegisterSettings& settings)
	{
		if constexpr (in.template has<Valid>())
			valid(in).resetValue('0');

		StreamT ret;

		if constexpr (in.template has<Ready>())
		{
			Bit valid_reg;
			auto dsSig = constructFrom(copy(downstream(in)));

			IF(!valid_reg | ready(in))
			{
				valid_reg = valid(in);
				dsSig = downstream(in);
			}

			valid_reg = reg(valid_reg, '0', settings);
			dsSig = reg(dsSig, settings);

			downstream(ret) = dsSig;
			upstream(in) = upstream(ret);
			ready(in) |= !valid_reg;
		}
		else
		{
			downstream(ret) = reg(copy(downstream(in)));
			upstream(in) = upstream(ret);
		}
		return ret;
	}

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

	template<StreamSignal StreamT>
	StreamT synchronizeStreamReqAck(StreamT& in, const Clock& inClock, const Clock& outClock)
	{
		Area area("synchronizeStreamReqAck", true);
		ClockScope csIn{ inClock };
		auto crossingStream = in
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

		StreamT out = crossingStream
			.add(Ready{})
			.add(Valid{})
			.template reduceTo<StreamT>();

		Bit outValid;
		outValid = flag(outputEnableCondition, outValid & ready(out));
		valid(out) = outValid;

		ready(in) = synchronizeEvent(transfer(out), outClock, inClock);

		return out;
	}

	template<Signal T, Signal ... Meta, StreamSignal StreamT>
	StreamT lookup(StreamT& addr, Memory<T>& memory)
	{
		auto out = addr.transform([&](const UInt& address) {
			return memory[address].read();
			});
		if (memory.readLatencyHint())
		{
			for (size_t i = 0; i < memory.readLatencyHint() - 1; ++i)
				out <<= scl::strm::regDownstreamBlocking(move(out), { .allowRetimingBackward = true });
			return strm::regDownstream(move(out),{ .allowRetimingBackward = true });
		}
		return out;
	}

	template<StreamSignal StreamT>
	inline StreamT pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group)
	{
		if constexpr (in.template has<Valid>())
			valid(in).resetValue('0');

		StreamT ret;
		downstream(ret) = group(copy(downstream(in)));
		upstream(in) = upstream(ret);
		return ret;
	}

	template<StreamSignal T>
	T pipeinput(T& stream, PipeBalanceGroup& group) 
	{
		return scl::strm::pipeinputDownstream(move(stream), group);
	}
}

namespace gtry::scl {
	namespace internal
	{
		template<StreamSignal T>
		SimProcess performTransferWait(const T& stream, const Clock& clock) {
			co_await OnClk(clock);
		}

		template<StreamSignal T> requires (T::template has<Ready>() && !T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(ready(stream)));
		}

		template<StreamSignal T> requires (!T::template has<Ready>() && T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(valid(stream)));
		}

		template<StreamSignal T> requires (T::template has<Ready>() && T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(ready(stream)) || !simu(valid(stream)));
		}
	}
	SimProcess performTransferWait(const StreamSignal auto& stream, const Clock& clock) { return internal::performTransferWait(stream, clock); }

	template<StreamSignal T>
	SimProcess performTransfer(const T& stream, const Clock& clock) 
	{
		co_await OnClk(clock);
	}

	template<StreamSignal T> requires (T::template has<Valid>())
		SimProcess performTransfer(const T& stream, const Clock& clock) 
	{
		simu(valid(stream)) = '1';
		co_await performTransferWait(stream, clock);
		simu(valid(stream)) = '0';
	}
	namespace strm{using scl::performTransferWait;}
}
