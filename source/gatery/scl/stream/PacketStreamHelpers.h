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
		}

		SimPacket& operator =(std::span<const uint8_t> data) { payload = sim::createDefaultBitVectorState(data.size(), data.data()); return *this; }
		SimPacket& operator<<(const sim::DefaultBitVectorState& additionalData) { payload.append(additionalData); return *this; }

		SimPacket& txid(size_t id) { m_txid = id; return *this; }
		SimPacket& error(bool err) { m_error = err; return *this; }
		SimPacket& invalidBeats(std::uint64_t invalidBeats) { m_invalidBeats = invalidBeats; return *this; }

		size_t txid() const { return m_txid; }
		bool error() const { return m_error; }
		std::uint64_t invalidBeats() const { return m_invalidBeats; }

		std::span<uint8_t> data() {
			HCL_DESIGNCHECK_HINT(payload.size() % 8, "Packet payload size is not a multiple of 8 bits!");
			return std::span<uint8_t>((uint8_t*)payload.data(sim::DefaultConfig::VALUE), payload.size() / 8);
		}
		operator std::span<uint8_t>() { return data(); }
	protected:
		size_t m_txid = 0;
		bool m_error = false;
		std::uint64_t m_invalidBeats = 0;
	};

	template<class... Meta>
	SimProcess sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket& packet, Clock clk);

	template<class... Meta>
	SimProcess sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket& packet, Clock clk, scl::SimulationSequencer& sequencer);

	template<class... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<BVec, Meta...>& stream, Clock clk, std::uint64_t unreadyBeatMask = 0);

	template<class... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<BVec, Meta...>& stream, Clock clk, std::uint64_t unreadyBeatMask, scl::SimulationSequencer& sequencer);

}



namespace gtry::scl
{

	template<class... Meta>
	SimProcess sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket& packet, Clock clk)
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

		size_t emptyBytes = 0;
		if constexpr (hasEmpty) {
			HCL_DESIGNCHECK_HINT((stream->size() % 8) == 0, "Stream payload width must be a whole number of bytes when using the empty signal");
			HCL_DESIGNCHECK_HINT((packet.payload.size() % 8) == 0, "Packet payload width must be a whole number of bytes when using the empty signal");
			size_t packetSizeBytes = packet.payload.size() / 8;
			size_t streamSizeBytes = stream->size() / 8;
			emptyBytes = streamSizeBytes - (packetSizeBytes % streamSizeBytes);
		}

		if constexpr (!hasTxId) {
			if(packet.txid() != 0)
				std::cout << "Warning: Trying to send a packet with a tx ID on a stream without a tx id field. Tx ID will be ignored" << std::endl;
		}

		if constexpr (!hasError) {
			if(packet.error())
				std::cout << "Warning: Trying to send a packet with an error flag on a stream without an error flag field. Error flag will be ignored" << std::endl;
		}

		std::uint64_t invalidBeatMask = packet.invalidBeats();

		HCL_DESIGNCHECK_HINT(invalidBeatMask == 0 || hasValid, "Can not produce bubbles on a stream without valid signal");

		for (size_t j = 0; j < numberOfBeats; j++) {
			size_t payloadOffset = j * stream->size();
			auto beatData = packet.payload.extract(payloadOffset, std::min(stream->size(), packet.payload.size() - payloadOffset));
			beatData.resize(stream->size());

			if constexpr (hasValid) {
				simu(valid(stream)) = '0';
				simu(*stream).invalidate();

				if constexpr (hasEop)
					simu(eop(stream)).invalidate();
				if constexpr (hasTxId)
					simu(txid(stream)).invalidate();
				if constexpr (hasSop)
					simu(sop(stream)).invalidate();
				if constexpr (hasError)
					simu(error(stream)).invalidate();
				if constexpr (hasTxId)
					simu(txid(stream)).invalidate();
				if constexpr (hasEmpty)
					simu(empty(stream)).invalidate();

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
				if (isLastBeat)
					simu(empty(stream)) = emptyBytes;

			}
			if constexpr (hasError) {
				simu(error(stream)).invalidate();
				if (isLastBeat)
					simu(error(stream)) = packet.error();
			}
			co_await performTransferWait(stream, clk);
		}

		if constexpr (hasEop)
			simu(eop(stream)) = '0';
		if constexpr (hasTxId)
			simu(txid(stream)).invalidate();
		if constexpr (hasSop)
			simu(sop(stream)) = '0';
		if constexpr (hasError)
			simu(error(stream)).invalidate();
		if constexpr (hasTxId)
			simu(txid(stream)).invalidate();
		if constexpr (hasEmpty)
			simu(empty(stream)).invalidate();

		if constexpr (hasValid) {
			simu(valid(stream)) = '0';
			if constexpr (hasEop)
				simu(eop(stream)).invalidate();
			if constexpr (hasSop)
				simu(sop(stream)).invalidate();
			if constexpr (hasEmpty)
				simu(empty(stream)).invalidate();
		}
		simu(*stream).invalidate();
	}

	template<class... Meta>
	SimProcess sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket& packet, Clock clk, scl::SimulationSequencer& sequencer) {
		auto slot = sequencer.allocate();
		co_await slot.wait();
		co_await sendPacket(stream, packet, clk);
	}

	template<class... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<BVec, Meta...>& stream, Clock clk, std::uint64_t unreadyBeatMask)
	{
		SimPacket result;

		constexpr bool hasEmpty = stream.template has<scl::Empty>();
		constexpr bool hasReady = stream.template has<scl::Ready>();
		constexpr bool hasError = stream.template has<scl::Error>();
		constexpr bool hasTxId = stream.template has<scl::TxId>();
		constexpr bool hasEop = stream.template has<scl::Eop>();


		HCL_DESIGNCHECK_HINT(unreadyBeatMask == 0 || hasReady, "Can not produce backpressure on a stream without ready signal");

		if constexpr (hasReady) {
			simu(ready(stream)) = '0';

			while (unreadyBeatMask & 1) {
				co_await OnClk(clk);
				unreadyBeatMask >>= 1;
			}
			unreadyBeatMask >>= 1;

			simu(ready(stream)) = '1';
		}

		co_await waitSop(stream, clk);
		bool needAwaitNextBeat = false;
		// fetch transaction-id and stuff
		if constexpr (hasTxId)
			result.txid(simu(txid(stream)));

		do {
			// when we start the loop, we don't want to await the next beat because we already awaited the sop
			if (needAwaitNextBeat) {
				if constexpr (hasReady) {
					simu(ready(stream)) = '0';

					while (unreadyBeatMask & 1) {
						co_await OnClk(clk);
						unreadyBeatMask >>= 1;
					}
					unreadyBeatMask >>= 1;

					simu(ready(stream)) = '1';
				}
				// wait for next beat
				co_await performTransferWait(stream, clk);
				if constexpr (hasTxId)
					BOOST_TEST(simu(txid(stream)) == result.txid());

			}
			needAwaitNextBeat = true;

			// fetch payload
			auto beatPayload = simu(*stream).eval();
			// Potentially crop off bytes in last beat
			if constexpr (hasEmpty)
				if (simuEop(stream)) {
					size_t numBytesToCrop = simu(empty(stream));
					//BOOST_TEST(numBytesToCrop < stream->size()/8);
					numBytesToCrop = std::min(numBytesToCrop, stream->size() / 8 - 1);
					beatPayload.resize(stream->size() - numBytesToCrop * 8);
				}
			if constexpr (hasError)
				if (simuEop(stream))
					result.error(simu(error(stream)));

			// Append to what we already have received
			result << beatPayload;
		} while (!simuEop(stream));

		if constexpr (hasReady)
			simu(ready(stream)) = '0';

		co_return result;
	}

	template<class... Meta>
	SimFunction<SimPacket> receivePacket(const scl::Stream<BVec, Meta...>& stream, Clock clk, std::uint64_t unreadyBeatMask, scl::SimulationSequencer& sequencer)
	{
		auto slot = sequencer.allocate();
		co_await slot.wait();

		auto packet = co_await receivePacket(stream, clk, unreadyBeatMask);
		co_return packet;
	}

}









