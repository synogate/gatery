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

namespace gtry::scl::strm
{
	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>)
	auto extendWidth(T&& source, BitWidth width, Bit reset = '0');

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload> and T::template has<Ready>())
	T reduceWidth(T&& source, BitWidth width, Bit reset = '0');

	template<StreamSignal T> 
	requires (T::template has<Ready>() and T::template has<Valid>())
	T eraseBeat(T&& source, UInt beatOffset, UInt beatCount);


	template<StreamSignal T, SignalValue Tval> 
	requires (T::template has<Ready>())
	T insertBeat(T&& source, UInt beatOffset, const Tval& value);

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T&& source, Bit stallCondition);

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stallPacket(T&& source, Bit stallCondition);

	template<BaseSignal Payload, Signal ... Meta, Signal... MetaInsert>
	auto insert(RvPacketStream<Payload, Meta...>&& base, RvStream<Payload, MetaInsert...>&& insert, RvStream<UInt>&& bitOffset);

	template<scl::StreamSignal TStream>
	TStream dropPacket(TStream&& in, Bit drop);
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
	auto extendWidth(T&& source, BitWidth width, Bit reset)
	{
		HCL_DESIGNCHECK(source->width() <= width);
		const size_t ratio = width / source->width();

		auto scope = Area{ "strm_extendWidth" }.enter();

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
		auto scope = Area{ "strm_reduceWidth" }.enter();
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
		auto scope = Area{ "strm_insertBeat" }.enter();
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
		return stall(std::move(source), stallCondition & sop(source));
	}

	template<BaseSignal Payload, Signal ... Meta, Signal... MetaInsert>
	auto insert(RvPacketStream<Payload, Meta...>&& base, RvStream<Payload, MetaInsert...>&& insert, RvStream<UInt>&& bitOffset)
	{
		Area ent{ "strm_streamInsert", true };
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
}
