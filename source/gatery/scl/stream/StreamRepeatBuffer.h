/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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
#include "metaSignals.h"
#include "../RepeatBuffer.h"


namespace gtry::scl::strm {

	struct RepeatBufferStreamSettings {
		/// The RepeatBuffer can hold at least that many data beats. The actual amount depends on the available target architecture.
		size_t minDepth = 16;
		/// How many beats the repeat buffer stores/repeats.
		UInt wrapAround;
		/// Optional control signal that can be strobed to emit on the read side the whole stored dataset.
		Bit releaseNextPacket = '1';
		/// On the write side, sync the write pointer to the sop of the input stream.
		bool wrResetOnWrSop = false;
		/// On the write side, automatically set update the wrap around mark to the eop of the input stream. Implies that only a single packet can be stored and repeated.
		bool setWarpAroundFromWrEop = false;
		/// On the read side, infer the eop of the output stream from the wrap around mark. Must be true, if setWarpAroundFromWrEop is true.
		bool inferRdEop = true;
	};

	/**
	* @brief Create a RepeatBuffer for repeating a sequence.
	* @param in The input stream.
	* @return connected stream
	*/
	template<StreamSignal StreamT>
	StreamT repeatBuffer(StreamT&& in, const RepeatBufferStreamSettings &settings = {});

	inline auto repeatBuffer(const RepeatBufferStreamSettings &settings = {})
	{
		return [=](auto&& in) { return repeatBuffer(std::forward<decltype(in)>(in), settings); };
	}

	/**
	 * @brief Returns a stream that is connected to the pop port of the RepeatBuffer.
	*/
	template<StreamSignal StreamT>
	StreamT pop(RepeatBuffer<typename StreamT::Payload>& repeatBuffer, const RepeatBufferStreamSettings &settings = {});
	template<StreamSignal StreamT>
	StreamT pop(RepeatBuffer<StreamData<StreamT>>& repeatBuffer, const RepeatBufferStreamSettings &settings = {});

	/**
	 * @brief Connects the in stream to the push port of the RepeatBuffer. Always ready.
	*/
	template<StreamSignal StreamT>
	void push(RepeatBuffer<typename StreamT::Payload>& repeatBuffer, StreamT&& in, const RepeatBufferStreamSettings &settings = {});
}


namespace gtry::scl::strm {

	template<StreamSignal StreamT>
	StreamT repeatBuffer(StreamT&& in, RepeatBuffer<StreamData<StreamT>>& instance, const RepeatBufferStreamSettings &settings)
	{
		StreamT ret = pop<StreamT>(instance, settings);
		push(instance, std::move(in), settings);
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT repeatBuffer(StreamT&& in, const RepeatBufferStreamSettings &settings)
	{
		RepeatBuffer<StreamData<StreamT>> inst{ settings.minDepth, in.removeFlowControl() };
		if (settings.wrapAround.valid())
			inst.wrapAround(settings.wrapAround);

		StreamT ret = repeatBuffer(move(in), inst, settings);
		return ret;
	}

	namespace internal {
		Bit buildValidLatch(Bit releaseNextPacket, Bit isLast, Bit streamReady);
	}

	template<StreamSignal StreamT>
	StreamT pop(RepeatBuffer<typename StreamT::Payload>& repeatBuffer, const RepeatBufferStreamSettings &settings)
	{
		StreamT ret = { repeatBuffer.peek() };
		
		if constexpr (StreamT::template has<Valid>()) {
			valid(ret) = internal::buildValidLatch(settings.releaseNextPacket, repeatBuffer.rdIsLast(), ready(ret));
		}
		if constexpr (StreamT::template has<Sop>()) sop(ret) = repeatBuffer.rdIsFirst();
		if constexpr (StreamT::template has<Eop>()) eop(ret) = repeatBuffer.rdIsLast();

		IF (transfer(ret))
			repeatBuffer.rdPop();
		return ret;
	}

	template<StreamSignal StreamT>
	StreamT pop(RepeatBuffer<StreamData<StreamT>>& repeatBuffer, const RepeatBufferStreamSettings &settings)
	{
		auto ret = repeatBuffer.rdPeek()
			.add(Ready{})
			.add(Valid{ '1' })
			.template reduceTo<StreamT>();

		valid(ret) = internal::buildValidLatch(settings.releaseNextPacket, repeatBuffer.rdIsLast(), ready(ret));

		if constexpr (StreamT::template has<Eop>()) 
			if (settings.inferRdEop)
				eop(ret) = repeatBuffer.rdIsLast();

		IF (transfer(ret))
			repeatBuffer.rdPop();
		return ret;
	}

	template<StreamSignal StreamT>
	void push(RepeatBuffer<typename StreamT::Payload>& repeatBuffer, StreamT&& in, const RepeatBufferStreamSettings &settings)
	{
		if constexpr (StreamT::template has<Ready>())
			ready(in) = '1';

		IF (transfer(in)) {				

			if constexpr (StreamT::template has<Eop>()) {
				if (settings.setWarpAroundFromWrEop) {
					HCL_DESIGNCHECK_HINT(!settings.wrapAround.valid(), "Creating a repeat buffer with an explicit wrapAround signal precludes inferring the wrap around from the input stream's eop!");

					IF (eop(in))
						repeatBuffer.wrWrapAround();
				} else
					sim_assert(eop(in) == repeatBuffer.wrIsLast()) << "eop of input stream should match wrap around of repeat buffer";
			}

			if (settings.wrResetOnWrSop) {
				IF (sop(in))
					repeatBuffer.wrReset();
			}

			repeatBuffer.wrPush(*in);
		}
	}

	template<StreamSignal StreamT>
	void push(RepeatBuffer<StreamData<StreamT>>& repeatBuffer, StreamT&& in, const RepeatBufferStreamSettings &settings)
	{
		if constexpr (StreamT::template has<Ready>())
			ready(in) = '1';

		IF (transfer(in)) {
			if constexpr (StreamT::template has<Eop>()) {
				if (settings.setWarpAroundFromWrEop) {
					HCL_DESIGNCHECK_HINT(!settings.wrapAround.valid(), "Creating a repeat buffer with an explicit wrapAround signal precludes inferring the wrap around from the input stream's eop!");

					IF (eop(in))
						repeatBuffer.wrWrapAround();
				} else
					sim_assert(eop(in) == repeatBuffer.wrIsLast()) << "eop of input stream should match wrap around of repeat buffer";
			}

			repeatBuffer.wrPush(in.removeFlowControl());
		}
	}
}
