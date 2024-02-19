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
#include "StreamBroadcaster.h"
#include "utils.h"
#include "../Counter.h"
#include "metaSignals.h"

namespace gtry::scl::strm
{
	template<class T>
	concept PacketStreamSignal = StreamSignal<T> and T::template has<Eop>();

	template<StreamSignal T>
	requires (T::template has<Valid>() or T::template has<Eop>())
	T eraseLastBeat(T& source);

	template<StreamSignal T> 
	auto addEopDeferred(T& source, Bit insert);

	auto addPacketSignalsFromCount(StreamSignal auto&& source, const UInt& size);

	template<Signal Payload, Signal ... Meta>
	RvPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const VPacketStream<Payload, Meta...>& source);

	template<Signal Payload, Signal ... Meta>
	RsPacketStream<Payload, Meta...> addReadyAndFailOnBackpressure(const SPacketStream<Payload, Meta...>& source);

	template<BaseSignal Payload, Signal... Meta>
	auto streamShiftLeft(Stream<Payload, Meta...>& in, UInt shift, Bit reset = '0');

	/**
	* @brief applies a shift on a packetStream's data. Also computes new EmptyBits signal. As a side-effect, it can also be used to get rid of packet headers.
	*/
	template<scl::strm::PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto streamShiftRight(StreamT&& source, UInt shift);

	UInt streamPacketBeatCounter(const StreamSignal auto& in, BitWidth counterW);

	template<BaseSignal Payload, Signal... Meta>
	UInt streamBeatBitLength(const Stream<Payload, Meta...>& in);

	/**
	* /!\ this module illegally has a combinatorial path between its output ready and output downstream signals. A regDownstream can be placed after it to remediate.
	* @brief inserts a stream into a packet stream with bit offset precision.
	* @param base stream into which data can be inserted
	* @param insert data stream to insert into main packet stream
	* @param bitOffset offset with which to start inserting data
	* @return same typed stream as base input
	*/
	template<BaseSignal Payload, Signal ... Meta, Signal... MetaInsert>
	auto insert(RvPacketStream<Payload, Meta...>&& base, RvStream<Payload, MetaInsert...>&& insert, RvStream<UInt>&& bitOffset);

	/**
	 * @brief extends the width of a packet stream and takes care of all the metaData signals. Limited to extensions by integer multiples of the original source width
	*/
	template<PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto widthExtend(StreamT&& source, const BitWidth& width);

	/**
	* @brief reduces the width of a packet stream and takes care of all the metaData signals. Limited to reductions by integer multiples of the original source width
	*/
	template<PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto widthReduce(StreamT&& source, const BitWidth& width);

	/**
	 * @brief generic version of widthReduce and widthExtend that will choose the right circuit accordingly
	*/
	template<PacketStreamSignal StreamT> requires std::is_base_of_v<BaseBitVector, typename StreamT::Payload>
	StreamT matchWidth(StreamT&& in, BitWidth desiredWidth);

}

namespace gtry::scl::strm
{

	template<StreamSignal T>
	class SimuStreamPerformTransferWait {
	public:
		SimProcess wait(const T& stream, const Clock& clock) {
			if constexpr (T::template has<Sop>()) {
				if (!m_isInPacket){
					do
						co_await performTransferWait(stream, clock);
					while (!simu(sop(stream)));
					m_isInPacket = true;
				}
				else{
					co_await performTransferWait(stream, clock);
					m_isInPacket = !(bool)simu(eop(stream));
				}
			}
			else {
				co_await performTransferWait(stream, clock);
			}
		}

	protected:
		bool m_isInPacket = false;
	};

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
		out = regDownstream(move(in));

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

		auto out = eraseLastBeat(in);
		insertState = flag(insert, transfer(out)) | insert;
		HCL_NAMED(out);
		return out;
	}

	auto addPacketSignalsFromCount(StreamSignal auto&& source, const UInt& size)
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
			Area ent{ "scl_addReadyAndFailOnBackpressure", true };
			Stream ret = source
				.add(Ready{})
				.add(Error{ error(source) });

			using Ret = decltype(ret);

			Bit hadError = flag(valid(source) & !ready(ret), transfer(ret) & eop(ret));
			HCL_NAMED(hadError);
			error(ret) |= hadError;

			// If we have an EOP and we are not ready, we try to generate an eop.
			// If there is no bubble to generate the eop, we discard the entire next packet.
			Bit hadUnhandledEop = flag(valid(source) & eop(source), transfer(ret));
			HCL_NAMED(hadUnhandledEop);
			IF(hadUnhandledEop & !valid(source))
			{
				if constexpr (Ret::template has<Valid>())
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
		using In = Stream<Payload, Meta...>;
		UInt len = in->width().bits();

		if constexpr (In::template has<scl::EmptyBits>())
		{
			IF(eop(in))
				len = in->width().bits() - zext(in.template get<scl::EmptyBits>().emptyBits);
		}
		else if constexpr (requires { empty(in); })
		{
			IF(eop(in))
			{
				UInt byteLen = in->width().bytes() - zext(empty(in));
				len = cat(byteLen, "b000");
			}
		}
		return len;
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
		auto baseShifted = streamShiftLeft(base, baseShift, baseShiftReset);	HCL_NAMED(baseShifted);
		auto insertShifted = streamShiftLeft(insert, insertBitOffset);		HCL_NAMED(insertShifted);
		Bit insertShiftedShouldDelayEop = valid(insert) & eop(insert) & zext(emptyBits(insert)) < zext(insertBitOffset); HCL_NAMED(insertShiftedShouldDelayEop);

		auto out = constructFrom(baseShifted);
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

	struct WidthManipMetaParams
	{
		const Counter& beat;
		Bit outEop;
		Bit outTransfer;
		size_t ratio;
	};
	
	Ready extendStreamMeta(Ready& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		Ready out;
		*in.ready = '1';
		IF(param.beat.isLast() | eop(inStream))
			*in.ready = *out.ready;
		return out;
	}

	Valid extendStreamMeta(Valid& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return { in.valid & (param.beat.isLast() | eop(inStream)) };
	}

	template<BitVectorSignal T>
	T extendStreamPayload(T& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		T ret = in.width() * param.ratio;
		ret = reg(ret);
		//UInt temp = zext(param.beat.value(), ret.width()) * UInt(in.width().bits()); //dumb overkill
		//HCL_NAMED(temp);
		//ret(temp, in.width()) = in;
		auto retParts = ret.parts(param.ratio);
		retParts[param.beat.value()] = in;
		return ret;
	}

	ByteEnable extendStreamMeta(ByteEnable& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		BitWidth byteEnableW = in.byteEnable.width();
		ByteEnable ret = { byteEnableW * param.ratio };
		ret = reg(ret);
		ret.byteEnable(param.beat.value() * byteEnableW.bits(), byteEnableW) = in.byteEnable;
		return ret;
	}

	Eop extendStreamMeta(Eop& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	Sop extendStreamMeta(Sop& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return { flagInstantSet(in.sop, param.beat.isLast() | eop(inStream)) };
	}

	Empty extendStreamMeta(Empty& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		size_t OutStartingPoint = inStream->width().bytes() * (param.ratio - 1);
		BitWidth outEmptyW = BitWidth::last(OutStartingPoint + in.empty.width().last());

		UInt empty = outEmptyW;

		empty -= inStream->width().bytes();

		IF(param.beat.isLast() | eop(inStream))
			empty = OutStartingPoint ;

		ENIF(transfer(inStream)) empty = reg(empty, OutStartingPoint);
		
		UInt ret = empty + zext(in.empty);
		IF(eop(inStream))
			empty = ret;

		return { ret };
	}

	Error extendStreamMeta(Error& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	TxId extendStreamMeta(TxId& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	EmptyBits extendStreamMeta(EmptyBits& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		size_t OutStartingPoint = inStream->width().bits() * (param.ratio - 1);
		BitWidth outEmptyBitsW = BitWidth::last(OutStartingPoint + in.emptyBits.width().last());

		UInt emptyBits = outEmptyBitsW;

		emptyBits -= inStream->width().bits();

		IF(param.beat.isLast() | eop(inStream))
			emptyBits = OutStartingPoint ;

		ENIF(transfer(inStream)) emptyBits = reg(emptyBits, OutStartingPoint);

		UInt ret = emptyBits + zext(in.emptyBits);
		IF(eop(inStream))
			emptyBits = ret;

		return { ret };
	}

	template <Signal SignalT> 
	SignalT extendStreamMeta(SignalT& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	template<PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto widthExtend(StreamT&& source, const BitWidth& width)
	{
		HCL_DESIGNCHECK(source->width() <= width);
		HCL_DESIGNCHECK_HINT((width.bits() % source->width().bits()) == 0, "currently, non-exact-multiple-extends are not supported");
		const size_t ratio = width / source->width();
		auto scope = Area{ "scl_extendWidth" }.enter();

		Counter counter{ ratio };
		IF(transfer(source))
			counter.inc();
		IF(transfer(source) & eop(source))
			counter.reset();

		const WidthManipMetaParams params{
			.beat = counter,
			.ratio = ratio,
		};

		StreamT ret = {
			.data = extendStreamPayload(*source, source, params),
			._sig = std::apply([&](auto& ...meta) {
				return std::tuple{ extendStreamMeta(meta, source, params)... };
			}, source._sig)
		};

		HCL_NAMED(ret);
		return ret;
	}


	Ready reduceStreamMeta(Ready& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		Ready out;
		*in.ready = '0';
		IF(param.beat.isLast() | param.outEop)
			*in.ready = *out.ready;
		return out;
	}

	Valid reduceStreamMeta(Valid& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return { in.valid };
	}

	template<BitVectorSignal T>
	T reduceStreamPayload(T& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		BitWidth outW = in.width() / param.ratio;
		T ret = outW;
		ret = in.part(param.ratio, param.beat.value());
		return ret;
	}

	ByteEnable reduceStreamMeta(ByteEnable& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		BitWidth outByteEnableW = in.byteEnable.width() / param.ratio;
		ByteEnable ret = { outByteEnableW };
		ret.byteEnable = in.byteEnable(param.beat.value() * outByteEnableW.bits(), outByteEnableW);
		return ret;
	}

	template <StreamSignal StreamT>
	Eop reduceStreamMeta(Eop& in, StreamT& inStream, const WidthManipMetaParams& param)
	{
		const size_t bitsPerBeatIn = inStream->width().bits();
		const size_t bitsPerBeatOut = bitsPerBeatIn / param.ratio;
		UInt fullBits = bitsPerBeatIn - zext(emptyBits(inStream));

		HCL_NAMED(fullBits);
		UInt sentBits = BitWidth(bitsPerBeatIn);

		IF(transfer(inStream)) sentBits = 0;
		sentBits += bitsPerBeatOut;
		ENIF(param.outTransfer) sentBits = reg(sentBits , bitsPerBeatOut);
		

		Bit isLastBeat = sentBits >= zext(fullBits);
		return { in.eop & isLastBeat };
	}

	Sop reduceStreamMeta(Sop& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return { in.sop & param.beat.isFirst()};
	}

	Empty reduceStreamMeta(Empty& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		const size_t bytesPerBeatIn = inStream->width().bytes();
		const size_t bytesPerBeatOut = bytesPerBeatIn / param.ratio;
		UInt bytesLeft = BitWidth::last(bytesPerBeatIn); 

		bytesLeft -= bytesPerBeatOut;
		IF(transfer(inStream)) bytesLeft = bytesPerBeatIn;
		ENIF(param.outTransfer) bytesLeft = reg(bytesLeft, bytesPerBeatIn);

		BitWidth emptyOutW = BitWidth::count(bytesPerBeatOut);
		return { (bytesLeft - zext(empty(inStream))).lower(emptyOutW)};
	}

	Error reduceStreamMeta(Error& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	TxId reduceStreamMeta(TxId& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	EmptyBits reduceStreamMeta(EmptyBits& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		const size_t bitsPerBeatIn = inStream->width().bits();
		const size_t bitsPerBeatOut = bitsPerBeatIn / param.ratio;
		UInt bitsLeft = BitWidth::last(bitsPerBeatIn); 

		bitsLeft -= bitsPerBeatOut;
		IF(transfer(inStream)) bitsLeft = bitsPerBeatIn;
		ENIF(param.outTransfer) bitsLeft = reg(bitsLeft, bitsPerBeatIn);

		BitWidth emptyBitsOutW = BitWidth::count(bitsPerBeatOut);
		return { (bitsLeft - zext(emptyBits(inStream))).lower(emptyBitsOutW)};
	}

	template <Signal SignalT>
	SignalT reduceStreamMeta(SignalT& in, StreamSignal auto& inStream, const WidthManipMetaParams& param)
	{
		return in;
	}

	template<PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto widthReduce(StreamT&& source, const BitWidth& width)
	{
		auto scope = Area{ "scl_reduceWidth" }.enter();

		HCL_DESIGNCHECK(source->width() >= width);
		HCL_DESIGNCHECK_HINT((source->width().bits() % width.bits() ) == 0, "currently, non-exact-multiple-reduces are not supported");
		const size_t ratio = source->width() / width;

		Counter counter{ ratio };

		WidthManipMetaParams params{
			.beat = counter,
			.ratio = ratio,
		};

		// removed naming of members to avoid issues with aggregate initialization
		auto ret = Stream{
			reduceStreamPayload(*source, source, params),
			std::apply([&](auto& ...meta) {
				return std::tuple{ reduceStreamMeta(meta, source, params)... };
			}, source._sig)
		};

		params.outEop = eop(ret);
		params.outTransfer = transfer(ret);

		IF(transfer(ret))
			counter.inc();

		IF(transfer(ret) & transfer(source))
			counter.reset();

		HCL_NAMED(ret);
		return ret;
	}

	template<PacketStreamSignal StreamT> requires std::is_base_of_v<BaseBitVector, typename StreamT::Payload>
	StreamT matchWidth(StreamT&& in, BitWidth desiredWidth) {
		if (desiredWidth > in.width())
			return widthExtend(move(in), desiredWidth);
		else if (desiredWidth < in.width())
			return widthReduce(move(in), desiredWidth);
		else if (desiredWidth == in.width())
			return move(in);
		else
			HCL_DESIGNCHECK_HINT(false, "something went terribly wrong if this failed");
	}

	struct ShiftRightMetaParams {
		UInt shift;
		Bit anticipateEnd;
		Bit outTransfer;
		Bit outEop;
		Bit canConsumeBeat;
	};

	template<BitVectorSignal T>
	T shiftRightPayload(T& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		T ret = constructFrom(in);

		ENIF(transfer(inStream)) {
			T doubleVec = (T) cat(in, reg(in));
			ret = doubleVec(param.shift, in.width());
		}

		return ret;
	}

	scl::Valid shiftRightMeta(scl::Valid& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		scl::Valid ret;
		ret.valid = in.valid;

		Bit thereWasAnAnticipatedEnd = scl::flagInstantSet(param.anticipateEnd, param.outTransfer & param.outEop,'0');
		HCL_NAMED(thereWasAnAnticipatedEnd);
		Bit validHeldForAnExtraBeat = !thereWasAnAnticipatedEnd & scl::flagInstantSet(transfer(inStream) & eop(inStream), param.outTransfer & param.outEop, '0');
		HCL_NAMED(validHeldForAnExtraBeat);

		IF(in.valid & sop(inStream)) {
			ret.valid = '0';
		}
		ret.valid |= validHeldForAnExtraBeat;
		return ret;
	}

	scl::Eop shiftRightMeta(scl::Eop& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		scl::Eop ret;

		Bit anticipatedEop = in.eop;
		Bit DelayedEop = reg(scl::flagInstantSet(in.eop, param.outTransfer, '0'), '0');

		ret.eop = DelayedEop;
		Bit delayedAnticipateEndSignal = reg(param.anticipateEnd);
		IF(param.anticipateEnd | delayedAnticipateEndSignal)
			ret.eop = anticipatedEop;

		return ret;
	}

	scl::Ready shiftRightMeta(scl::Ready& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		scl::Ready ret;
		Bit canConsumeBeat = valid(inStream) & sop(inStream);
		*in.ready = *ret.ready | canConsumeBeat;
		return ret;
	}

	scl::EmptyBits shiftRightMeta(scl::EmptyBits& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		HCL_DESIGNCHECK_HINT(std::popcount(inStream->width().bits()) == 1, "only for streams with powers of 2 data bus widths");

		scl::EmptyBits ret = { capture(in.emptyBits + zext(param.shift), valid(inStream) & eop(inStream)) };

		return ret;
	}



	template<Signal T>
	T shiftRightMeta(T& in, auto& inStream, const ShiftRightMetaParams& param)
	{
		T ret = move(in);
		//something something
		return ret;
	}

	template<scl::strm::PacketStreamSignal StreamT> requires (std::is_base_of_v<BaseBitVector, typename StreamT::Payload>)
	auto streamShiftRight(StreamT&& source, UInt shift)
	{
		auto scope = Area{ "scl_streamShiftRight" }.enter();

		ShiftRightMetaParams params{
			.shift = shift,
		};

		auto ret = StreamT{
			.data = shiftRightPayload(*source, source, params),
			._sig = std::apply([&](auto& ...meta) {
				return std::tuple{ shiftRightMeta(meta, source, params)... };
				}, source._sig)
		};

		UInt fullBits = source->width().bits() - zext(emptyBits(source));
		params.anticipateEnd = scl::flagInstantSet(valid(source) & eop(source) & (zext(shift) >= fullBits), transfer(ret) & eop(ret), '0');
		setName(params.anticipateEnd, "anticipateEnd");
		params.outTransfer = transfer(ret);
		params.outEop = eop(ret);

		HCL_NAMED(ret);
		return ret;
	}
}
