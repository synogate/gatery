/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include <gatery/scl/sim/SimulationSequencer.h>
#include <gatery/scl/stream/Stream.h>

namespace gtry::scl
{
	struct SimPacket {
		sim::DefaultBitVectorState payload;

		SimPacket() = default;
		SimPacket(std::span<const uint8_t> data) { *this = data; }
		SimPacket(uint64_t payload, BitWidth payloadW) {
			HCL_DESIGNCHECK_HINT(BitWidth::last(payload) <= payloadW, "The selected payload width would result in data truncation. Design not allowed" );
			size_t numberOfBytesInPayload = payloadW.numBeats(8_b);
			for (size_t byteIterator = 0; byteIterator < numberOfBytesInPayload; byteIterator++) {
				this->payload.append(sim::createDefaultBitVectorState(1, payload & 0xFF));
				payload >>= 8;
			}
			this->payload.resize(payloadW.bits());
		}

		SimPacket& operator =(std::span<const uint8_t> data) { payload = sim::createDefaultBitVectorState(data.size(), data.data()); return *this; }
		SimPacket& operator<<(const sim::DefaultBitVectorState& additionalData) { payload.append(additionalData); return *this; }

		SimPacket& txid(size_t id) { m_txid = id; return *this; }
		SimPacket& error(char err) { m_error = err; return *this; }
		SimPacket& invalidBeats(std::uint64_t invalidBeats) { m_invalidBeats = invalidBeats; return *this; }

		size_t txid() const { return m_txid; }
		char error() const { return m_error; }
		std::uint64_t invalidBeats() const { return m_invalidBeats; }

		std::span<uint8_t> data() {
			HCL_DESIGNCHECK_HINT(payload.size() % 8 == 0, "Packet payload size is not a multiple of 8 bits!");
			return std::span<uint8_t>((uint8_t*)payload.data(sim::DefaultConfig::VALUE), payload.size() / 8);
		}
		operator std::span<uint8_t>() { return data(); }
	protected:
		size_t m_txid = 0;
		char m_error = '0';
		std::uint64_t m_invalidBeats = 0;
	};

	template<BaseSignal Payload, Signal... Meta>
	SimProcess sendPacket(const scl::Stream<Payload, Meta...>& stream, SimPacket packet, Clock clk);

	template<BaseSignal Payload, Signal... Meta>
	SimProcess sendPacket(const scl::Stream<Payload, Meta...>& stream, SimPacket packet, Clock clk, scl::SimulationSequencer& sequencer);

	template<BaseSignal Payload, Signal... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<Payload, Meta...>& stream, Clock clk);

	template<BaseSignal Payload, Signal... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<Payload, Meta...>& stream, Clock clk, scl::SimulationSequencer& sequencer);

	template<BaseSignal Payload, Signal... Meta>
	SimProcess readyDriver(const scl::Stream<Payload, Meta...>& stream, Clock clk, const uint64_t& unreadyMask = 0);

	template<BaseSignal Payload, Signal... Meta>
	SimProcess readyDriverRNG(const scl::Stream<Payload, Meta...>& stream, Clock clk, size_t readyProbabilityPercent, unsigned int seed = 1234);

	template<BaseSignal Payload, Signal... Meta>
	void simuStreamInvalidate(const scl::Stream<Payload, Meta...>& stream);
}



namespace gtry::scl
{
	template<BaseSignal Payload, Signal... Meta>
	void simuStreamInvalidate(const scl::Stream<Payload, Meta...>& stream) {

		if constexpr (stream.template has<scl::Eop>())
			simu(eop(stream)) = '0';
		if constexpr (stream.template has<scl::Sop>())
			simu(sop(stream)) = '0';
		if constexpr (stream.template has<scl::TxId>())
			simu(txid(stream)).invalidate();
		if constexpr (stream.template has<scl::Error>())
			simu(error(stream)).invalidate();
		if constexpr (stream.template has<scl::Empty>())
			simu(empty(stream)).invalidate();

		simu(*stream).invalidate();

		if constexpr (stream.template has<scl::Valid>()) {
			simu(valid(stream)) = '0';
			if constexpr (stream.template has<scl::Eop>())
				simu(eop(stream)).invalidate();
			if constexpr (stream.template has<scl::Sop>())
				simu(sop(stream)).invalidate();
		}
	}

	template<BaseSignal Payload, Signal... Meta>
	SimProcess sendPacket(const scl::Stream<Payload, Meta...>& stream, SimPacket packet, Clock clk)
	{
		size_t numberOfBeats = packet.payload.size() / stream->size();
		if (((packet.payload.size()) % stream->size()) != 0)
			numberOfBeats++;

		constexpr bool hasError = stream.template has<scl::Error>();
		constexpr bool hasTxId	= stream.template has<scl::TxId>();
		constexpr bool hasValid = stream.template has<scl::Valid>();
		constexpr bool hasSop	= stream.template has<scl::Sop>();
		constexpr bool hasEmpty = stream.template has<scl::Empty>();
		constexpr bool hasEop = stream.template has<scl::Eop>();

		HCL_DESIGNCHECK_HINT(hasEop || numberOfBeats == 1, "Trying to send multi-beat data packets without an End of Packet Field");

		if constexpr (!hasTxId) {
			HCL_DESIGNCHECK_HINT(packet.txid() == 0, "It is not allowed to send a packet with a tx ID on a stream without a tx ID field");
		}

		if constexpr (!hasError) {
			HCL_DESIGNCHECK_HINT(packet.error() == '0', "It is not allowed to send a packet with an error on a stream without an error field");
		}

		std::uint64_t invalidBeatMask = packet.invalidBeats();

		HCL_DESIGNCHECK_HINT(invalidBeatMask == 0 || hasValid, "Can not produce bubbles on a stream without valid signal");

		for (size_t j = 0; j < numberOfBeats; j++) {
			size_t payloadOffset = j * stream->size();
			auto beatData = packet.payload.extract(payloadOffset, std::min(stream->size(), packet.payload.size() - payloadOffset));
			beatData.resize(stream->size());

			simuStreamInvalidate(stream);
			
			if constexpr (hasValid) {
				simu(valid(stream)) = '0';
				while (invalidBeatMask & 1) {
					co_await OnClk(clk);
					invalidBeatMask >>= 1;
				}
				invalidBeatMask >>= 1;
				simu(valid(stream)) = '1';
			}
			simu(*stream) = beatData;

			if constexpr (hasSop)
				simu(sop(stream)) = j == 0;

			if constexpr (hasTxId)
				simu(txid(stream)) = packet.txid();

			bool isLastBeat = j == numberOfBeats - 1;
			if constexpr (hasEop)
				simu(eop(stream)) = isLastBeat;

			if constexpr (hasEmpty) {
				simu(empty(stream)).invalidate();
				if (isLastBeat){
					HCL_DESIGNCHECK_HINT((stream->size() % 8) == 0, "Stream payload width must be a whole number of bytes when using the empty signal");
					HCL_DESIGNCHECK_HINT((packet.payload.size() % 8) == 0, "Packet payload width must be a whole number of bytes when using the empty signal");
					size_t packetSizeBytes = packet.payload.size() / 8;
					size_t streamSizeBytes = stream->size() / 8;
					simu(empty(stream)) = streamSizeBytes - (packetSizeBytes % streamSizeBytes);
				}
			}
			if constexpr (hasError) {
				simu(error(stream)).invalidate();
				if (isLastBeat)
					simu(error(stream)) = packet.error();
			}
			co_await performTransferWait(stream, clk);
		}
		simuStreamInvalidate(stream);
	}

	template<BaseSignal Payload, Signal... Meta>
	SimProcess sendPacket(const scl::Stream<Payload, Meta...>& stream, SimPacket packet, Clock clk, scl::SimulationSequencer& sequencer) {
		auto slot = sequencer.allocate();
		co_await slot.wait();
		co_await sendPacket(stream, packet, clk);
	}

	template<BaseSignal Payload, Signal... Meta>
	SimProcess readyDriver(const scl::Stream<Payload, Meta...>& stream, Clock clk, const uint64_t& unreadyMask){
		static_assert(stream.template has<Ready>(), "Attempting to use a ready driver on a stream which does not feature a ready field is probably a mistake.");

		simuReady(stream) = '0';
		while (simuSop(stream) != '1' || simuValid(stream) != '1') {
			co_await OnClk(clk);
		}
		uint64_t unreadyMaskCopy = unreadyMask;
		while (true) {
			simuReady(stream) = (unreadyMaskCopy & 1 ) ? '0' : '1';
			unreadyMaskCopy >>= 1;
			if (simuEop(stream) == '1' && simuReady(stream) == '1') {
				unreadyMaskCopy = unreadyMask;
			}
			co_await OnClk(clk);
		}
	}

	template<BaseSignal Payload, Signal... Meta>
	SimProcess readyDriverRNG(const scl::Stream<Payload, Meta...>& stream, Clock clk, size_t readyProbabilityPercent, unsigned int seed) {
		static_assert(stream.template has<Ready>(), "Attempting to use a ready driver on a stream which does not feature a ready field is probably a mistake.");
		assert(readyProbabilityPercent <= 100);

		simuReady(stream) = '0';
		while (simuSop(stream) != '1' || simuValid(stream) != '1') {
			co_await OnClk(clk);
		}

		std::mt19937 gen(seed);
		std::uniform_int_distribution<> distrib(0,99);

		while (true) {
			simuReady(stream) = distrib(gen) < readyProbabilityPercent;
			co_await OnClk(clk);
		}
	}
			
	template<BaseSignal Payload, Signal... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<Payload, Meta...>& stream, Clock clk)
	{
		scl::SimuStreamPerformTransferWait<scl::Stream<Payload, Meta...>> streamTransfer;
		SimPacket result;

		constexpr bool hasEmpty = stream.template has<scl::Empty>();
		constexpr bool hasError = stream.template has<scl::Error>();
		constexpr bool hasTxId = stream.template has<scl::TxId>();

		bool firstBeat = true;

		do {
			co_await streamTransfer.wait(stream, clk);

			if constexpr (hasTxId) {
				if (firstBeat) {
					result.txid(simu(txid(stream)));
					firstBeat = false;
				}
			}
			auto beatPayload = simu(*stream).eval();

			// Potentially crop off bytes in last beat
			if constexpr (hasEmpty)
				if (simuEop(stream)) {
					size_t numBytesToCrop = simu(empty(stream));
					numBytesToCrop = std::min(numBytesToCrop, stream->size() / 8 - 1);
					beatPayload.resize(stream->size() - numBytesToCrop * 8);
				}
			if constexpr (hasError)
				if (simuEop(stream))
					result.error(simu(error(stream)));

			result << beatPayload;
		} while (!simuEop(stream));

		co_return result;
	}

	template<BaseSignal Payload, Signal... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<Payload, Meta...>& stream, Clock clk, scl::SimulationSequencer& sequencer)
	{
		auto slot = sequencer.allocate();
		co_await slot.wait();

		auto packet = co_await receivePacket(stream, clk);
		co_return packet;
	}
}









