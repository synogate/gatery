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
#include "Packet.h"

namespace gtry::scl::strm
{
	/**
	 * @brief The SimPacket is a data structure used to represent packets in Gatery during simulation. 
	 * In order to send a simulation packet through a stream the user must initialize a SimPacket of
	 * the desired packet, using one of the ctor helpers provided.
	*/
	struct SimPacket {
		gtry::sim::DefaultBitVectorState payload;

		SimPacket() = default;
		SimPacket(gtry::sim::DefaultBitVectorState payload) : payload(std::move(payload)) {}
		SimPacket(std::span<const uint8_t> data) { *this = data; }
		SimPacket(std::span<const std::byte> data) { *this = data; }
		SimPacket(uint64_t payload, BitWidth payloadW) {
			HCL_DESIGNCHECK_HINT(BitWidth::last(payload) <= payloadW, "The selected payload width would result in data truncation. Design not allowed" );
			size_t numberOfBytesInPayload = payloadW.numBeats(8_b);
			for (size_t byteIterator = 0; byteIterator < numberOfBytesInPayload; byteIterator++) {
				this->payload.append(gtry::sim::createDefaultBitVectorState(8, payload & 0xFF));
				payload >>= 8;
			}
			this->payload.resize(payloadW.bits());
		}

		SimPacket& operator =(std::span<const std::byte> data) { payload = gtry::sim::createDefaultBitVectorState(data.size() * 8, data.data()); return *this; }
		SimPacket& operator =(std::span<const uint8_t> data) { payload = gtry::sim::createDefaultBitVectorState(data.size() * 8, data.data()); return *this; }
		SimPacket& operator<<(const gtry::sim::DefaultBitVectorState& additionalData) { payload.append(additionalData); return *this; }

		SimPacket& txid(size_t id) { m_txid = id; return *this; }
		SimPacket& error(char err) { m_error = err; return *this; }
		SimPacket& invalidBeats(std::uint64_t invalidBeats) { m_invalidBeats = invalidBeats; return *this; }

		size_t txid() const { return m_txid; }
		uint64_t asUint64(BitWidth numberOfLSBsToConvert) const { return payload.extractNonStraddling(gtry::sim::DefaultConfig::VALUE, 0, numberOfLSBsToConvert.bits()); }
		char error() const { return m_error; }
		std::uint64_t invalidBeats() const { return m_invalidBeats; }

		std::span<std::byte> asBytes() {
			HCL_DESIGNCHECK_HINT(payload.size() % 8 == 0, "Packet payload size is not a multiple of 8 bits!");
			return std::span<std::byte>((std::byte*)payload.data(gtry::sim::DefaultConfig::VALUE), payload.size() / 8);
		}
		std::span<uint8_t> data() {
			HCL_DESIGNCHECK_HINT(payload.size() % 8 == 0, "Packet payload size is not a multiple of 8 bits!");
			return std::span<uint8_t>((uint8_t*)payload.data(gtry::sim::DefaultConfig::VALUE), payload.size() / 8);
		}
		operator std::span<uint8_t>() { return data(); }
		operator std::span<std::byte>() { return asBytes(); }

		bool operator == (const SimPacket& other) const { return payload == other.payload && m_txid == other.m_txid && m_error == other.m_error; }
	protected:
		size_t m_txid = 0;
		char m_error = '0';
		std::uint64_t m_invalidBeats = 0;
	};

	template<StreamSignal StreamT>
	SimProcess sendBeat(const StreamT& stream, size_t payload, const Clock& clk);

	/**
	 * @brief Helper for sending a Beat of data through a stream during simulation. Thanks to the use of a sequencer,
	 * the beats will be scheduled to be sent in the chronological order in which the function is called.
	 * @param stream Stream onto which to schedule the beat to be sent.
	 * @param payload Unsigned integer representation of the payload to be sent.
	 * @param clk Clock scope of the stream.
	 * @param sequencer Sequencer used to schedule event.
	 * @return a co_await-able SimProcess
	*/
	template<StreamSignal StreamT>
	SimProcess sendBeat(const StreamT& stream, size_t payload, const Clock& clk, SimulationSequencer& sequencer);

	template<StreamSignal StreamT>
	SimProcess sendPacket(const StreamT& stream, SimPacket packet, Clock clk);

	template<StreamSignal StreamT>
	SimProcess sendPacket(const StreamT& stream, SimPacket packet, Clock clk, scl::SimulationSequencer& sequencer);

	template<StreamSignal StreamT>
	SimFunction<SimPacket> receivePacket(const StreamT& stream, Clock clk);

	template<StreamSignal StreamT>
	SimFunction<SimPacket> receivePacket(const StreamT& stream, Clock clk, scl::SimulationSequencer& sequencer);

	template<StreamSignal StreamT>
	SimProcess readyDriver(const StreamT& stream, Clock clk, const uint64_t unreadyMask = 0);

	template<StreamSignal StreamT>
	SimProcess readyDriverRNG(const StreamT& stream, Clock clk, size_t readyProbabilityPercent, unsigned int seed = 1234);

	template<StreamSignal StreamT>
	void simuStreamInvalidate(const StreamT& stream);
}

namespace gtry::scl::strm
{
	template<StreamSignal T>
	SimProcess performTransferWait(const T& stream, const Clock& clock)
	{
		co_await OnClk(clock);
	}

	template<StreamSignal T> requires (T::template has<Ready>() && !T::template has<Valid>())
	SimProcess performTransferWait(const T& stream, const Clock& clock)
	{
		do
			co_await OnClk(clock);
		while (!simu(ready(stream)));
	}

	template<StreamSignal T> requires (!T::template has<Ready>() && T::template has<Valid>())
	SimProcess performTransferWait(const T& stream, const Clock& clock)
	{
		do
			co_await OnClk(clock);
		while (!simu(valid(stream)));
	}

	template<StreamSignal T> requires (T::template has<Ready>() && T::template has<Valid>())
	SimProcess performTransferWait(const T& stream, const Clock& clock)
	{
		do
			co_await OnClk(clock);
		while (!simu(ready(stream)) || !simu(valid(stream)));
	}

	template<StreamSignal StreamT>
	SimProcess performPacketTransferWait(const StreamT& stream, const Clock& clock)
	{
		do
			co_await OnClk(clock);
		while (!(simuValid(stream) == '1' && simuReady(stream) == '1' && simuEop(stream) == '1'));
	}

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

	template<StreamSignal StreamT>
	void simuStreamInvalidate(const StreamT& stream) {

		if constexpr (StreamT::template has<Eop>())
			simu(eop(stream)) = '0';
		if constexpr (StreamT::template has<Sop>())
			simu(sop(stream)) = '0';
		if constexpr (StreamT::template has<TxId>())
			simu(txid(stream)).invalidate();
		if constexpr (StreamT::template has<Error>())
			simu(error(stream)).invalidate();
		if constexpr (StreamT::template has<Empty>())
			simu(empty(stream)).invalidate();
		if constexpr (StreamT::template has<EmptyBits>())
			simu(stream.template get<EmptyBits>().emptyBits).invalidate();

		simu(*stream).invalidate();

		if constexpr (StreamT::template has<Valid>()) {
			simu(valid(stream)) = '0';
			if constexpr (StreamT::template has<Eop>())
				simu(eop(stream)).invalidate();
			if constexpr (StreamT::template has<Sop>())
				simu(sop(stream)).invalidate();
		}
	}

	template<StreamSignal StreamT>
	SimProcess sendBeat(const StreamT& stream, size_t payload, const Clock& clk)
	{
		BitWidth payloadW = width(*stream);
		payload &= payloadW.mask();
		return strm::sendPacket(stream, SimPacket{ payload, payloadW }, clk);
	}

	template<StreamSignal StreamT>
	SimProcess sendBeat(const StreamT& stream, size_t payload, const Clock& clk, SimulationSequencer& sequencer)
	{
		BitWidth payloadW = width(*stream);
		payload &= payloadW.mask();
		return strm::sendPacket(stream, SimPacket{ payload, payloadW }, clk, sequencer);
	}

	template<StreamSignal StreamT>
	SimProcess sendPacket(const StreamT& stream, SimPacket packet, Clock clk)
	{
		size_t payloadBeatBits = width(*stream).bits();
		size_t numberOfBeats = packet.payload.size() / payloadBeatBits;
		if ((packet.payload.size() % payloadBeatBits) != 0)
			numberOfBeats++;

		constexpr bool hasError = StreamT::template has<scl::Error>();
		constexpr bool hasTxId	= StreamT::template has<scl::TxId>();
		constexpr bool hasValid = StreamT::template has<scl::Valid>();
		constexpr bool hasSop	= StreamT::template has<scl::Sop>();
		constexpr bool hasEop   = StreamT::template has<scl::Eop>();

		if constexpr (!hasTxId) {
			HCL_DESIGNCHECK_HINT(packet.txid() == 0, "It is not allowed to send a packet with a tx ID on a stream without a tx ID field");
		}

		if constexpr (!hasError) {
			HCL_DESIGNCHECK_HINT(packet.error() == '0', "It is not allowed to send a packet with an error on a stream without an error field");
		}

		std::uint64_t invalidBeatMask = packet.invalidBeats();

		HCL_DESIGNCHECK_HINT(invalidBeatMask == 0 || hasValid, "Can not produce bubbles on a stream without valid signal");

		for (size_t j = 0; j < numberOfBeats; j++) {
			size_t payloadOffset = j * payloadBeatBits;
			auto beatData = packet.payload.extract(payloadOffset, std::min(payloadBeatBits, packet.payload.size() - payloadOffset));
			beatData.resize(payloadBeatBits);

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

			if constexpr (StreamT::template has<scl::EmptyBits>())
			{
				const UInt& emptyBits = stream.template get<scl::EmptyBits>().emptyBits;
				simu(emptyBits).invalidate();
				if (isLastBeat) {
					simu(emptyBits) = stream->size() - (packet.payload.size() % stream->size());
					if (packet.payload.size() % stream->size() == 0)
						simu(emptyBits) = 0;

				}
			}
			else if constexpr (StreamT::template has<scl::Empty>())
			{
				simu(empty(stream)).invalidate();
				if (isLastBeat) {
					HCL_DESIGNCHECK_HINT((payloadBeatBits % 8) == 0, "Stream payload width must be a whole number of bytes when using the empty signal");
					HCL_DESIGNCHECK_HINT((packet.payload.size() % 8) == 0, "Packet payload width must be a whole number of bytes when using the empty signal");
					size_t packetSizeBytes = packet.payload.size() / 8;
					size_t streamSizeBytes = stream->size() / 8;
					size_t leftOvers = packetSizeBytes % streamSizeBytes;
					size_t emptyRet = streamSizeBytes - leftOvers;
					if (leftOvers == 0) emptyRet = 0; // this adaptation allows for non-power-of-2 streams to work 
					simu(empty(stream)) = emptyRet;
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

	template<StreamSignal StreamT>
	SimProcess sendPacket(const StreamT& stream, SimPacket packet, Clock clk, scl::SimulationSequencer& sequencer) {
		auto slot = sequencer.allocate();
		co_await slot.wait();
		co_await sendPacket(stream, packet, clk);
	}

	template<StreamSignal StreamT>
	SimProcess readyDriver(const StreamT& stream, Clock clk, const uint64_t unreadyMask){

#ifdef __APPLE__
		if constexpr (!StreamT::template has<Ready>())
			throw std::logic_error("Attempting to use a ready driver on a stream which does not feature a ready field is probably a mistake.");
#else
		static_assert(StreamT::template has<Ready>(), "Attempting to use a ready driver on a stream which does not feature a ready field is probably a mistake.");
#endif

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

	template<StreamSignal StreamT>
	SimProcess readyDriverRNG(const StreamT& stream, Clock clk, size_t readyProbabilityPercent, unsigned int seed) {
		static_assert(StreamT::template has<Ready>(), "Attempting to use a ready driver on a stream which does not feature a ready field is probably a mistake.");
		assert(readyProbabilityPercent <= 100);

		simuReady(stream) = '0';
		while (simuSop(stream) != '1' || simuValid(stream) != '1') {
			co_await OnClk(clk);
		}

		std::mt19937 gen(seed);
		std::uniform_int_distribution<size_t> distrib(0,99);

		while (true) {
			simuReady(stream) = distrib(gen) < readyProbabilityPercent;
			co_await OnClk(clk);
		}
	}

	template<StreamSignal StreamT>
	SimFunction<SimPacket> receivePacket(const StreamT& stream, Clock clk)
	{
		strm::SimuStreamPerformTransferWait<StreamT> streamTransfer;
		SimPacket result;

		bool firstBeat = true;
		do {
			co_await streamTransfer.wait(stream, clk);
			auto beatPayload = simu(*stream).eval();

			if (firstBeat) 
			{
				firstBeat = false;
				if constexpr (StreamT::template has<scl::TxId>())
					result.txid(simu(txid(stream)));
			}

			if (simuEop(stream))
			{
				// Potentially crop off bytes in last beat
				if constexpr (StreamT::template has<scl::EmptyBits>())
				{
					size_t numBitsToCrop = simu(stream.template get<scl::EmptyBits>().emptyBits);
					HCL_DESIGNCHECK(numBitsToCrop < beatPayload.size());
					beatPayload.resize(beatPayload.size() - numBitsToCrop);
				}
				else if constexpr (StreamT::template has<scl::Empty>())
				{
					size_t numBytesToCrop = simu(empty(stream));
					HCL_DESIGNCHECK(numBytesToCrop * 8 < beatPayload.size());
					beatPayload.resize(beatPayload.size() - numBytesToCrop * 8);
				}

				if constexpr (StreamT::template has<scl::Error>())
					result.error(simu(error(stream)));
			}

			result << beatPayload;
		} while (!simuEop(stream));

		co_return result;
	}

	template<StreamSignal StreamT>
	SimFunction<SimPacket> receivePacket(const StreamT& stream, Clock clk, scl::SimulationSequencer& sequencer)
	{
		auto slot = sequencer.allocate();
		co_await slot.wait();

		auto packet = co_await receivePacket(stream, clk);
		co_return packet;
	}
}









