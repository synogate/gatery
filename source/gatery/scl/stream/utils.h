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
#include "Stream.h"
#include "metaSignals.h"
#include "../cdc.h"
#include "StreamBroadcaster.h"
#include "StreamArbiter.h"

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

	inline auto regDownstream(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDownstream(std::forward<decltype(in)>(in), settings); };
	}

	template<StreamSignal StreamT>
	StreamT delay(StreamT&& in, size_t cycles, const RegisterSettings& settings = {});

	inline auto delay(size_t cycles, const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return delay(std::forward<decltype(in)>(in), cycles, settings); };
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

	inline auto eraseBeat(UInt beatOffset, UInt beatCount)
	{
		return [=](auto&& source) { return eraseBeat(std::forward<decltype(source)>(source), beatOffset, beatCount); };
	}

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
	//requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T&& source, Bit stallCondition);

	inline auto stall(Bit stallCondition)
	{
		return [=](auto&& source) { return stall(std::forward<decltype(source)>(source), stallCondition); };
	}

	template<StreamSignal StreamT> 
		requires (StreamT::template has<Ready>() and StreamT::template has<Valid>())
	StreamT allowanceStall(StreamT&& source, Bit allow, BitWidth allowanceW, size_t initialAllowance = 0);

	inline auto allowanceStall(Bit allow, BitWidth allowanceW, size_t initialAllowance = 0)
	{
		return [=](auto&& source) { return allowanceStall(std::forward<decltype(source)>(source), allow, allowanceW, initialAllowance); };
	}
		
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

	inline auto dropPacket(Bit drop)
	{
		return [=](auto&& in) { return dropPacket(std::forward<decltype(in)>(in), drop); };
	}

	/**
	* @brief Puts a register in the ready, valid and data path.
	* @param stream Source stream
	* @param Settings forwarded to all instantiated registers.
	* @return connected stream
	*/
	template<StreamSignal T> 
	T regDecouple(T& stream, const RegisterSettings& settings = {});
	template<StreamSignal T> 
	T regDecouple(T&& stream, const RegisterSettings& settings = {}) { return regDecouple(stream, settings); }
	template<StreamSignal T>
	T regDecouple(const T& stream, const RegisterSettings& settings = {});

	//untested
	inline auto regDecouple(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDecouple(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief allows to send a request-acknowledge handshaked data across clock domains
	* @param in input stream with data to pass across clock domains 
	* @param inClock clock domain of input stream
	* @param outClock clock domain of returned stream
	*/
	template<StreamSignal StreamT>
	StreamT synchronizeStreamReqAck(StreamT& in, const Clock& inClock, const Clock& outClock);

	/**
	 * @brief This helper creates a stream whose payload is SignalT, a metaSignal of the original stream
	 * @param input stream
	 * @return stream with SignalT as Payload
	*/
	template<Signal SignalT, StreamSignal StreamT> requires (StreamT::template has<SignalT>())
	auto extractMeta(StreamT&& in);

	/**
	 * @brief   This helper returns a stream whose payload is the raw (packed BVec) version of its original payload
	 * @return  stream with BVec as payload
	*/
	template<StreamSignal StreamT> 
	auto rawPayload(StreamT&& in);

	/**
	* @brief attaches a memory to a stream carrying addresses and returns a stream of the memory data
	* @param addr the address stream to look up in memory
	* @param memory - self explanatory
	*/
	template<StreamSignal StreamT, Signal Tout>
	auto lookup(StreamT&& addr, Memory<Tout>& memory);

	template<Signal Tout>
	inline auto lookup(Memory<Tout>& memory)
	{
		return [&memory](auto&& in) { return lookup(std::forward<decltype(in)>(in), memory); };
	}

	/**
	* @brief	Puts a register spawner for retiming in the valid and data path.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group);

	//untested
	inline auto pipeinputDownstream(PipeBalanceGroup& group)
	{
		return [&group](auto&& in) { return pipeinputDownstream(std::forward<decltype(in)>(in), group); };
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

	inline auto pipeinput()
	{
		return [=](auto&& source) { return pipeinput(std::forward<decltype(source)>(source)); };
	}

	template<StreamSignal StreamT>
	Vector<StreamT> serialPushParallelPopBuffer(StreamT&& in, size_t numberOfElements);

	/**
	 * @brief Hides eop of numPacketsToCombine - 1 packets to combine numPacketsToCombine packets into one.
	 * @param numPacketsToCombine number of packets to combine to one. Do not change during packets.
	*/
	template<StreamSignal StreamT>
	StreamT combinePackets(StreamT&& in, UInt numPacketsToCombine);
	/**
	 * @brief creates a VStream with the payload and valid bits you give it
	*/
	template<Signal Tp>
	VStream<std::remove_cvref_t<Tp>> createVStream(Tp&& payload, const Bit& validBit) { VStream<std::remove_cvref_t<Tp>> ret(std::forward<Tp>(payload)); valid(ret) = validBit; return ret; }



	/**
	 * @brief Stalls both streams to synchonize them, such that every beat on the second stream is lined up and kept constant for all beats of a corresponding packet on the first stream.
	 * @details If the second stream is not a packet stream, the two streams are simply aligned beat for beat.
	 * @returns The input streams, but stalling each other to be aligned.
	 */
	template<StreamSignal BeatStreamT, StreamSignal PacketStreamT>
	std::tuple<PacketStreamT, BeatStreamT> replicateForEntirePacket(PacketStreamT &&packetStream, BeatStreamT &&beatStream);


	/**
	 * @brief Computes a meta signal from each packet using the given functor and attaches the result as a meta signal.
	 * @details The functor returns an RvStream to indicate, when it has completed its computation. It must produce one beat
	 * for each ingested packet (or for each ingested beat if the input is not a packet stream).
	 * The packet stream is duplicated into a fifo to ensure on the output side that the meta signal can be joined with the packet such that it is stable
	 * for the entire duration of the packet.
	 * @param inputStream The packet stream to processs and return
	 * @param maxPacketLength The depth of the fifo, which must be at least as big as the maximum packet size in beats.
	 * @param functor A lambda transforming the input packet stream into a stream of meta information structs.
	 * @returns A delayed variant of the input stream with the computed meta information attached and stable between sop and eop.
	 */
	template<StreamSignal InStreamT, typename Functor>
	auto addMetaSignalFromPacket(InStreamT &&inputStream, size_t maxPacketLength, Functor functor);

	/**
	 * @brief Computes a meta signal from each packet using the given functor and attaches the result as a meta signal.
	 * @details The functor returns an RvStream to indicate, when it has completed its computation. It must produce one beat
	 * for each ingested packet (or for each ingested beat if the input is not a packet stream).
	 * The packet stream is duplicated into a fifo to ensure on the output side that the meta signal can be joined with the packet such that it is stable
	 * for the entire duration of the packet.
	 * @param maxPacketLength The depth of the fifo, which must be at least as big as the maximum packet size in beats.
	 * @param functor A lambda transforming the input packet stream into a stream of meta information structs.
	 */
	template<typename Functor>
	inline auto addMetaSignalFromPacket(size_t maxPacketLength, Functor functor) {
		return [maxPacketLength, functor](auto&& in) { return addMetaSignalFromPacket(std::forward<decltype(in)>(in), maxPacketLength, functor); };
	}

	/**
	 * @brief Short hand for creating a StreamArbiter. Streams are arbitrated in the order they are passed to the function.
	 * @param selector Arbitration policy to use. See StreamArbiter for more information.
	 * @param in1 First stream to arbitrate.
	 * @param ...inX Any number of additional streams to arbitrate with.
	 */
	template<typename Policy, StreamSignal T, StreamSignal... To>
	T arbitrateWithPolicy(Policy&& selector, T&& in1, To&&... inX);

	/**
	 * @brief Short hand for creating a StreamArbiter. Streams are arbitrated in the order they are passed to the function. Same as arbitrateWithPolicy, but with ArbiterPolicyLowest policy.
	 * @param in1 First stream to arbitrate.
	 * @param ...inX Any number of additional streams to arbitrate with.
	 */
	template<StreamSignal T, StreamSignal... To>
	T arbitrate(T&& in1, To&&... inX);

	/**
	* @brief this module does not comply with valid semantics. It can change its payload without transfer,
	* but this might be acceptable since its use case is only to prevent catastrophic lockup and to give
	* proper failure reporting possibilities to user, such as the possibility to mapOut the totalLostPackets.
	*/
	template<StreamSignal StreamT> requires (!StreamT::template has<Ready>() && !StreamT::template has<Eop>())
	std::tuple<decltype(attach(StreamT{}, Ready{})), UInt> addReadyAndCompensateForLostBeats(StreamT&& in, BitWidth counterW);

	/**
	 * @brief This overload allows for stream chaining. Please pass in an uninitialized UInt.
	*/
	inline auto addReadyAndCompensateForLostBeats(BitWidth counterW, UInt& lostBeatCount) {
		return [=, &lostBeatCount](auto&& source) {
			auto [outStream, outLostBeatCount] = addReadyAndCompensateForLostBeats(move(source), counterW);
			lostBeatCount = outLostBeatCount;
			return move(outStream);
		};
	}

	/**
	* @brief This overload allows for stream chaining
	*/
	inline auto addReadyAndCompensateForLostBeats(BitWidth counterW){
		return [=](auto&& source) { 
			auto [outStream, outLostBeatCount] = addReadyAndCompensateForLostBeats(move(source), counterW);
			return move(outStream);
		};
	}

	/**
	 * @brief Merge payload and all meta signals of two streams into one. There is no predefined semantics or default behavior for how to merge the signals. See join for an example.
	 * @param payloadOp Used to merge the payload of the two streams.
	 * @param metaBiOp Used to merge two meta signals. In case both streams have the same meta signal.
	 * @param metaUnOp Used to transform any meta signal that exists in only one of the two streams.
	 */
	template<StreamSignal S1, StreamSignal S2, typename PayloadOp, typename MetaBiOp, typename MetaUnOp>
	auto merge(S1&& s1, S2&& s2, PayloadOp&& payloadOp, MetaBiOp&& metaBiOp, MetaUnOp&& metaUnOp);

	template<StreamSignal S2, typename PayloadOp, typename MetaBiOp, typename MetaUnOp>
	auto merge(S2&& s2, PayloadOp&& payloadOp, MetaBiOp&& metaBiOp, MetaUnOp&& metaUnOp)
	{
		return [&](auto&& s1) { return merge(std::forward<decltype(s1)>(s1), std::forward<decltype(s2)>(s2), move(payloadOp), move(metaBiOp), move(metaUnOp)); };
	};

	template<typename ...TOperatorTypes>
	struct Operators : TOperatorTypes... { using TOperatorTypes::operator ()...; };

	namespace internal
	{
		struct MakePair;
	}
	struct JoinBeat;
	struct JoinNothing;
	struct MergeTakeFirst;

	/**
	 * @brief Used to synchronize two streams using back pressure or credit based flow control. It will not fail on streams that do not have flow control.
	 * @param joinOp Used to merge the payload of the two streams. The default is std::pair.
	 * @param metaJoinOp Used resolve conflicts of unhandled meta signals that appear in both streams.
	 */
	template<typename JoinOp = internal::MakePair, typename MetaJoinOp = JoinNothing, StreamSignal S1, StreamSignal S2> requires(!StreamSignal<JoinOp> and !StreamSignal<MetaJoinOp>)
	auto join(S1&& s1, S2&& s2, JoinOp&& joinOp = JoinOp{}, MetaJoinOp&& metaJoinOp = MetaJoinOp{});

	template<typename JoinOp = internal::MakePair, typename MetaJoinOp = JoinNothing, StreamSignal S2>  requires(!StreamSignal<JoinOp> and !StreamSignal<MetaJoinOp>)
	auto join(S2&& s2, JoinOp&& joinOp = JoinOp{}, MetaJoinOp&& metaJoinOp = MetaJoinOp{})
	{
		return [&](auto&& s1) { return join(std::forward<decltype(s1)>(s1), std::forward<decltype(s2)>(s2), move(joinOp), move(metaJoinOp)); };
	};
}

namespace gtry::scl::strm
{
	template<StreamSignal StreamT>
	inline StreamT regDownstreamBlocking(StreamT&& in, const RegisterSettings& settings)
	{
		if constexpr (StreamT::template has<Valid>())
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
		if constexpr (StreamT::template has<Valid>())
			valid(in).resetValue('0');
		if constexpr (StreamT::template has<Ready>())
			ready(in).resetValue('0');

		StreamT ret = constructFrom(in);
		ret <<= in;

		if constexpr (StreamT::template has<Ready>())
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
		if constexpr (StreamT::template has<Valid>())
			valid(in).resetValue('0');

		StreamT ret;

		if constexpr (StreamT::template has<Ready>())
		{
			Bit valid_reg;
			auto dsSig = constructFrom(copy(downstream(in)));

			IF(ready(in))
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


	template<StreamSignal StreamT>
	StreamT delay(StreamT&& in, size_t cycles, const RegisterSettings& settings) {
		StreamT ret = std::forward<StreamT>(in);

		if(cycles > 0)
		{
			for (size_t i = 0; i < cycles - 1; i++)
				ret = regDownstreamBlocking(move(ret), settings);
			ret = regDownstream(move(ret), settings);
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

		auto ret = attach(
			move(source),
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
		*out = source->part(ratio, counter.value());

		if constexpr (T::template has<ByteEnable>())
		{
			BVec& be = byteEnable(out);
			BitWidth w = be.width() / ratio;
			be.resetNode();
			be = byteEnable(source)(zext(counter.value(), +w) * w.bits(), w);
		}

		if constexpr (T::template has<Eop>())
			eop(out) &= counter.isLast();
		if constexpr (T::template has<Sop>())
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

	template<Signal SignalT, StreamSignal StreamT> requires (StreamT::template has<SignalT>()) 
		auto extractMeta(StreamT&& in) {
		return in.transform([&](auto&& payload) { return get<SignalT>(in); }).template remove<SignalT>();
	}

	template<StreamSignal StreamT> 
	auto rawPayload(StreamT&& in) {
		return in.transform([](auto&& payload) { return (BVec)pack(payload); });
	}

	template<StreamSignal T>
	//	requires (T::template has<Ready>() and T::template has<Valid>())
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

	template<StreamSignal StreamT> 
		requires (StreamT::template has<Ready>() and StreamT::template has<Valid>())
	StreamT allowanceStall(StreamT&& source, Bit allow, BitWidth allowanceW, size_t initialAllowance) {
		Area area{ "scl_allowance_stall", true };
		HCL_DESIGNCHECK(BitWidth::last(initialAllowance) <= allowanceW);

		Bit stallCondition; HCL_NAMED(stallCondition);
		StreamT stalledSource = move(source) | scl::strm::stall(stallCondition);

		Counter allowance(allowanceW, initialAllowance);
		IF(allow)
			allowance.inc();
		IF(transfer(stalledSource))
			allowance.dec();

		const Bit initialStall = initialAllowance == 0 ? '1' : '0';
		stallCondition = reg(allowance.becomesFirst(), initialStall);
		return stalledSource;
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
		static_assert(!T::template has<Ready>(), "cannot create upstream register from const stream");
		return strm::regDownstream(stream, settings);
	}

	template<StreamSignal StreamT>
	StreamT synchronizeStreamReqAck(StreamT& in, const Clock& inClock, const Clock& outClock)
	{
		Area area("synchronizeStreamReqAck", true);
		ClockScope csIn{ inClock };
		auto crossingStream = move(in)
			| remove<Ready>()
			| remove<Valid>();

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

		StreamT out = move(crossingStream)
			| attach(Ready{})
			| attach(Valid{})
			| reduceTo<StreamT>();

		Bit outValid;
		outValid = flag(outputEnableCondition, outValid & ready(out));
		valid(out) = outValid;

		ready(in) = synchronizeEvent(transfer(out), outClock, inClock);

		return out;
	}

	template<StreamSignal StreamT, Signal Tout>
	auto lookup(StreamT&& addr, Memory<Tout>& memory) 
	{
		return std::forward<StreamT>(addr) | 
			transform([&](const auto& address) { return memory[address].read(); }) |
			delay(memory.readLatencyHint(), { .allowRetimingBackward = true });
	}

	template<StreamSignal StreamT>
	inline StreamT pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group)
	{
		if constexpr (StreamT::template has<Valid>())
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

	template<StreamSignal StreamT>
	Vector<StreamT> serialPushParallelPopBuffer(StreamT&& in, size_t numberOfElements) 
	{
		Vector<StreamT> popStreams;
		Vector<StreamT> shiftStreams;

		popStreams.reserve(numberOfElements);
		shiftStreams.reserve(numberOfElements);

		shiftStreams.push_back(move(in));

		for (size_t i = 0; i < numberOfElements; i++)
		{
			StreamT popStream(constructFrom(*in));

			IF(transfer(popStream))
				valid(popStream) = '0';

			IF(transfer(shiftStreams.back()))
				downstream(popStream) = downstream(shiftStreams.back());

			downstream(popStream) = reg(copy(downstream(popStream))); // copy gets rid of the references returned by the downstream function


			StreamT shiftStream(*popStream);
			valid(shiftStream) = valid(popStream);
			ready(shiftStreams.back()) = !valid(popStream) | ready(popStream) | ready(shiftStream);

			StreamT temp = strm::stall(move(shiftStream), transfer(popStream));

			StreamT temp2(*popStream);
			valid(temp2) = valid(popStream);
			ready(popStream) = ready(temp2);
			popStreams.push_back(move(temp2));

			IF(transfer(temp))
				valid(popStream) = '0';

			shiftStreams.push_back(move(temp)); 
		}
		ready(shiftStreams.back()) = '0';
		return popStreams;
	}

	template<StreamSignal StreamT>
	StreamT combinePackets(StreamT&& in, UInt numPacketsToCombine)
	{
		scl::Counter ctr{ numPacketsToCombine };
		IF(transfer(in) & eop(in))
			ctr.inc();

		StreamT out = move(in);
		eop(out) &= ctr.isLast();
		return out;
	}


	template<StreamSignal BeatStreamT, StreamSignal PacketStreamT>
	std::tuple<PacketStreamT, BeatStreamT> replicateForEntirePacket(PacketStreamT &&packetStream, BeatStreamT &&beatStream)
	{
		BeatStreamT outBeatStream = constructFrom(beatStream);
		PacketStreamT outPacketStream = constructFrom(packetStream);

		// Stall packet stream if we don't have anything on the beat stream
		outPacketStream <<= move(packetStream) | stall(!valid(beatStream)); 
		// Stall the beat stream until the eop of the packet stream. Compose the transfer(packetStream) from the valid of the input and the ready of the output to not have a dependency on valid(beatStream)
		outBeatStream <<= move(beatStream) | stall(!(valid(packetStream) & ready(outPacketStream) & eop(packetStream)));
		// But make the beat stream "duplicate" its output for the entire duration of the packet, i.e.
		valid(outBeatStream) = valid(beatStream) & valid(packetStream);

		return { move(outPacketStream), move(outBeatStream) };
	}

	template<StreamSignal InStreamT, typename Functor>
	auto addMetaSignalFromPacket(InStreamT &&inputStream, size_t maxPacketLength, Functor functor)
	{
		Area area("addMetaSignalFromPacket", true);
		HCL_NAMED(inputStream);

		StreamBroadcaster bcast(move(inputStream));
		auto metaStream = functor(bcast.bcastTo());
		HCL_NAMED(metaStream);

		auto resultStream = bcast.bcastTo() 
			| fifo(maxPacketLength) 
			| attach(move(metaStream));
		HCL_NAMED(resultStream);
		return resultStream;
	}

	template<typename Policy, StreamSignal T, StreamSignal... To>
	T arbitrateWithPolicy(Policy&& selector, T&& in1, To&&... inX)
	{
		StreamArbiter<T, Policy> arbiter{ move(selector) };
		arbiter.attach(in1);
		(arbiter.attach(inX), ...);
		arbiter.generate();

		T out;
		out <<= arbiter.out();
		return out;
	}

	template<StreamSignal StreamT> requires (!StreamT::template has<Ready>() && !StreamT::template has<Eop>())
	std::tuple<decltype(attach(StreamT{}, Ready{})), UInt> addReadyAndCompensateForLostBeats(StreamT&& in, BitWidth counterW) {
		Area area("scl_addReadyAndCompensateForLostBeats", true);

		auto inWithReady = attach(move(in), Ready{});
		Counter totalLostBeats(counterW);
		Counter lostBeats(counterW);

		Bit lostBeat = valid(inWithReady) & !ready(inWithReady);
		sim_debugIf(lostBeat) << __FILE__ << " " << __LINE__ << " this beat, the packet has been lost, but it will be compensated with a garbage beat in the future";
		IF(lostBeat) {
			lostBeats.inc();
			totalLostBeats.inc();
		}

		Bit garbageBeat = !lostBeats.isFirst() & !valid(inWithReady);
		IF(garbageBeat) {
			valid(inWithReady) |= '1';
			*inWithReady = allZeros(*inWithReady);
		}

		IF(garbageBeat & transfer(inWithReady))
			lostBeats.dec();

		return std::make_tuple(move(inWithReady), totalLostBeats.value());
	}

	template<StreamSignal T, StreamSignal... To>
	T arbitrate(T&& in1, To&&... inX)
	{
		return arbitrateWithPolicy(ArbiterPolicyLowest{}, move(in1), move(inX)...);
	}

	namespace internal
	{
		template<typename Tuple, typename Result = std::tuple<>>
		struct remove_duplicates;

		template<typename Result>
		struct remove_duplicates<std::tuple<>, Result>
		{
			using type = Result;
		};

		template<typename T, typename... Ts, typename... Rs>
		struct remove_duplicates<std::tuple<T, Ts...>, std::tuple<Rs...>>
		{
			using type = std::conditional_t<
				std::disjunction_v<std::is_same<T, Rs>...>,
				typename remove_duplicates<std::tuple<Ts...>, std::tuple<Rs...>>::type,
				typename remove_duplicates<std::tuple<Ts...>, std::tuple<Rs..., T>>::type
			>;
		};

		struct MakePair
		{
			template<typename ...T>
			auto operator()(T&&... t) const
			{
				return std::pair(std::forward<T>(t)...);
			};
		};
	}

	struct MergeEmpty {};

	template<StreamSignal S1, StreamSignal S2, typename PayloadOp, typename MetaBiOp, typename MetaUnOp>
	auto merge(S1&& s1, S2&& s2, PayloadOp&& payloadOp, MetaBiOp&& metaBiOp, MetaUnOp&& metaUnOp)
	{
		using NewMeta = internal::remove_duplicates<decltype(s2._sig), decltype(s1._sig)>::type;

		auto metaJoin = [&](const auto& typeRef) {
			using MetaT = std::remove_cvref_t<decltype(typeRef)>;
			if constexpr (std::remove_cvref_t<S1>::template has<MetaT>() and std::remove_cvref_t<S2>::template has<MetaT>())
				return metaBiOp(
					get<std::remove_cvref_t<decltype(typeRef)>>(std::forward<S1>(s1)),
					get<std::remove_cvref_t<decltype(typeRef)>>(std::forward<S2>(s2)),
					std::forward<S1>(s1),
					std::forward<S2>(s2)
				);
			else if constexpr (std::remove_cvref_t<S1>::template has<MetaT>())
				return metaUnOp(
					get<std::remove_cvref_t<decltype(typeRef)>>(std::forward<S1>(s1)),
					MergeEmpty{},
					std::forward<S1>(s1),
					std::forward<S2>(s2)
				);
			else
				return metaUnOp(
					MergeEmpty{},
					get<std::remove_cvref_t<decltype(typeRef)>>(std::forward<S2>(s2)),
					std::forward<S1>(s1),
					std::forward<S2>(s2)
				);
			};

		return Stream{
			payloadOp(
				*std::forward<S1>(s1),
				*std::forward<S2>(s2),
				std::forward<S1>(s1),
				std::forward<S2>(s2)
			),
			std::apply([&](const auto&... sigs) {
				return std::tuple(metaJoin(sigs)...);
			}, NewMeta{})
		};
	}

	struct JoinBeat
	{
		template<typename T> requires(std::is_same_v<T, Valid>)
		T operator ()(T&& a, T&& b, auto&&, auto&&) const
		{
			return { a.valid & b.valid };
		}

		Ready operator ()(Ready&& a, Ready&& b, StreamSignal auto&& sa, StreamSignal auto&& sb) const
		{
			Ready ret;
			*a.ready = *ret.ready & valid(sb);
			*b.ready = *ret.ready & valid(sa);
			return ret;
		}

		Ready operator ()(Ready&& a, MergeEmpty&&, const StreamSignal auto& sa, const StreamSignal auto& sb) const
		{
			Ready ret;
			*a.ready = *ret.ready & valid(sb);
			return ret;
		}

		Ready operator ()(MergeEmpty&&, Ready&& b, const StreamSignal auto& sa, const StreamSignal auto& sb) const
		{
			Ready ret;
			*b.ready = *ret.ready & valid(sa);
			return ret;
		}

		Credit operator ()(Credit&& a, Credit&& b, auto&&, auto&&) const
		{
			Credit ret{
				.initialCredit = std::min(a.initialCredit, b.initialCredit),
				.maxCredit = std::max(a.maxCredit, b.maxCredit)
			};
			*a.increment = *ret.increment;
			*b.increment = *ret.increment;
			return ret;
		}
	};

	struct MergeTakeFirst
	{
		template<typename T>
		T operator ()(T&& a, const T&, auto&&, auto&&) const
		{
			return std::forward<decltype(a)>(a);
		}

		auto operator ()(auto&& a, MergeEmpty&&, const auto&, const auto&) const
		{
			return std::forward<decltype(a)>(a);
		}

		auto operator ()(MergeEmpty&&, auto&& b, const auto&, const auto&) const
		{
			return std::forward<decltype(b)>(b);
		}
	};

	struct JoinNothing
	{
		bool operator ()() const = delete;
	};

	template<typename JoinOp, typename MetaJoinOp, StreamSignal S1, StreamSignal S2> requires(!StreamSignal<JoinOp> and !StreamSignal<MetaJoinOp>)
	auto join(S1&& s1, S2&& s2, JoinOp&& joinOp, MetaJoinOp&& metaJoinOp)
	{
		return merge(
			std::forward<S1>(s1),
			std::forward<S2>(s2),
			[&](auto&& a, auto&& b, const auto&, const auto&) {
				return joinOp(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b));
			},
			Operators<JoinBeat, MetaJoinOp>{},
			Operators<JoinBeat, MergeTakeFirst>{}
		);
	}
}
