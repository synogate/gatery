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

namespace gtry::scl
{

	class StreamTransferFixture : public BoostUnitTestSimulationFixture
	{
	protected:
		struct SimPacket {
			sim::DefaultBitVectorState payload;

			SimPacket() = default;
			SimPacket(const std::span<uint8_t> data) { *this = data; }
			SimPacket(const std::vector<uint8_t> &data) { *this = data; }
			
			SimPacket& operator =(const std::span<uint8_t> data) { payload = sim::createDefaultBitVectorState(data.size(), data.data()) ; return *this; }
			SimPacket& operator =(const std::vector<uint8_t>& data) { payload = sim::createDefaultBitVectorState(data.size(), data.data()) ; return *this; }
			SimPacket& operator<<(const sim::DefaultBitVectorState &additionalData) { payload.append(additionalData); return *this; }

			SimPacket& txid(size_t id) { m_txid = id; return *this; }
			SimPacket& error(bool err) { m_error = err; return *this; }
			SimPacket& invalidBeats(std::uint64_t invalidBeats) { m_invalidBeats = invalidBeats; return *this; }

			std::optional<size_t> txid() const { return m_txid; }
			std::optional<bool> error() const { return m_error; }
			std::uint64_t invalidBeats() const { return m_invalidBeats; }

			std::span<uint8_t> data() { 
				HCL_DESIGNCHECK_HINT(payload.size() % 8, "Packet payload size is not a multiple of 8 bits!");
				return std::span<uint8_t>((uint8_t*)payload.data(sim::DefaultConfig::VALUE), payload.size() / 8);
			}
			operator std::span<uint8_t>() { return data(); }
			protected:
				std::optional<size_t> m_txid;
				std::optional<bool> m_error;
				std::uint64_t m_invalidBeats = 0;
		};

		Clock m_clock = Clock({ .absoluteFrequency = 100'000'000 });

		void simulateTransferTest(scl::StreamSignal auto& source, scl::StreamSignal auto& sink);

		template<scl::StreamSignal T>
		void simulateArbiterTestSink(T& sink);

		template<scl::StreamSignal T>
		void simulateArbiterTestSource(T& source);

		void In(scl::StreamSignal auto& stream, std::string prefix = "in");

		void Out(scl::StreamSignal auto& stream, std::string prefix = "out");

		void simulateBackPressure(scl::StreamSignal auto& stream);

		template<class... Meta>
		void simulateSendData(scl::Stream<UInt, Meta...>& stream, size_t group);

		template<class... Meta>
		SimProcess sendDataPacket(scl::Stream<UInt, Meta...>& stream, size_t group, size_t packetOffset, size_t packetLen, uint64_t invalidBeats = 0);

		template<class... Meta>
		SimProcess sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket &packet, scl::SimulationSequencer& test, Clock clk);

		template<class... Meta>
		SimFunction<SimPacket> receivePacket(const scl::Stream<BVec, Meta...>& stream, scl::SimulationSequencer& test, Clock clk, std::uint64_t unreadyBeatMask = 0);

		template<class... Meta>
		void simulateSendData(scl::RsPacketStream<UInt, Meta...>& stream, size_t group);

		template<scl::StreamSignal T>
		void simulateRecvData(const T& stream);

	private:
		size_t m_groups = 0;
		size_t m_transfers = 16;
	};



	void StreamTransferFixture::simulateTransferTest(scl::StreamSignal auto& source, scl::StreamSignal auto& sink)
	{
		simulateBackPressure(sink);
		simulateSendData(source, m_groups++);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void StreamTransferFixture::simulateArbiterTestSink(T& sink)
	{
		simulateBackPressure(sink);
		simulateRecvData(sink);
	}

	template<scl::StreamSignal T>
	void StreamTransferFixture::simulateArbiterTestSource(T& source)
	{
		simulateSendData(source, m_groups++);
	}

	void StreamTransferFixture::In(scl::StreamSignal auto& stream, std::string prefix)
	{
		pinIn(stream, prefix);
	}

	void StreamTransferFixture::Out(scl::StreamSignal auto& stream, std::string prefix)
	{
		pinOut(stream, prefix);
	}

	void StreamTransferFixture::simulateBackPressure(scl::StreamSignal auto& stream)
	{
		auto recvClock = ClockScope::getClk();

		addSimulationProcess([&, recvClock]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };

			simu(ready(stream)) = '0';
			do
				co_await WaitStable();
			while (simu(valid(stream)) == '0');

			// todo: not good
			co_await WaitFor(Seconds{1,10} / recvClock.absoluteFrequency());

			while (true)
			{
				simu(ready(stream)) = rng() % 2 != 0;
				co_await AfterClk(recvClock);
			}
		});
	}

	template<class... Meta>
	void StreamTransferFixture::simulateSendData(scl::Stream<UInt, Meta...>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for(size_t i = 0; i < m_transfers;)
			{
				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				co_await sendDataPacket(stream, group, i, packetLen, rng());
				i += packetLen;
			}
			simu(valid(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<class... Meta>
	SimProcess StreamTransferFixture::sendDataPacket(scl::Stream<UInt, Meta...>& stream, size_t group, size_t packetOffset, size_t packetLen, uint64_t invalidBeats)
	{
		constexpr bool hasValid = stream.template has<scl::Valid>();
		for (size_t j = 0; j < packetLen; ++j)
		{
			simu(eop(stream)).invalidate();
			simu(*stream).invalidate();
			
			if constexpr (hasValid)
			{
				simu(valid(stream)) = '0';
				while ((invalidBeats & 1) != 0)
				{
					co_await AfterClk(m_clock);
					invalidBeats >>= 1;
				}
				simu(valid(stream)) = '1';
			}
			else
			{
				simu(sop(stream)) = j == 0;
			}

			simu(eop(stream)) = j == packetLen - 1;
			simu(*stream) = packetOffset + j + group * m_transfers;

			co_await scl::performTransferWait(stream, m_clock);
		}

		if(!hasValid)
			simu(sop(stream)) = '0';
	}

	template<class... Meta>
	SimProcess StreamTransferFixture::sendPacket(const scl::Stream<BVec, Meta...>& stream, const SimPacket &packet, scl::SimulationSequencer& test, Clock clk)
	{
		//HCL_ASSERT_HINT((stream->size() % 8) == 0, "Payload width must be a whole number of bytes");
		HCL_DESIGNCHECK_HINT(stream.template has<scl::Eop>(), "Every packet stream uses an EOP");
		//TODO: wait for your turn?
		
		size_t numberOfBeats = packet.payload.size() / stream->size();
		if (((packet.payload.size()) % stream->size()) != 0)
			numberOfBeats++;

		constexpr bool hasError = stream.template has<scl::Error>();
		constexpr bool hasTxId	= stream.template has<scl::TxId>();
		constexpr bool hasValid = stream.template has<scl::Valid>();
		constexpr bool hasSop	= stream.template has<scl::Sop>();
		constexpr bool hasEmpty = stream.template has<scl::Empty>();
		size_t emptyBytes = 0;
		if constexpr (hasEmpty) {
			HCL_DESIGNCHECK_HINT((stream->size() % 8) == 0, "Stream payload width must be a whole number of bytes when using the empty signal");
			HCL_DESIGNCHECK_HINT((packet.payload.size() % 8) == 0, "Packet payload width must be a whole number of bytes when using the empty signal");
			size_t packetSizeBytes = packet.payload.size() / 8;
			size_t streamSizeBytes = stream->size() / 8;
			emptyBytes = streamSizeBytes - (packetSizeBytes % streamSizeBytes);
		}

		if constexpr (hasTxId) {
			HCL_DESIGNCHECK_HINT(packet.txid(), "Packets send on a stream with tx IDs must have a tx id set");
		} else {
			HCL_DESIGNCHECK_HINT(!packet.txid(), "Packet has a tx id set, but the stream doesn't have a tx id field");
		}

		if constexpr (hasError){
			HCL_DESIGNCHECK_HINT(packet.error(), "Packets send on a stream with error flags must have an error flag set");
		} else {
			HCL_DESIGNCHECK_HINT(!packet.error(), "Packet has an error flag set, but the stream doesn't have an error field");
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
				simu(txid(stream)) = *packet.txid();

			bool isLastBeat = j == numberOfBeats - 1;
			simu(eop(stream)) = isLastBeat;

			if constexpr (hasEmpty) {
				simu(empty(stream)).invalidate();
				if (isLastBeat) 
					simu(empty(stream)) = emptyBytes;
				
			}
			if constexpr (hasError) {
				simu(error(stream)).invalidate();
				if (isLastBeat) 
					simu(error(stream)) = *packet.error();
			}
			co_await performTransferWait(stream, clk);
		}
		
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
			simu(eop(stream)).invalidate();
			if constexpr (hasSop) 
				simu(sop(stream)).invalidate();

			if constexpr (hasEmpty) 
				simu(empty(stream)).invalidate();
		}
		simu(*stream).invalidate();
	}
	template<class... Meta>
	SimFunction<StreamTransferFixture::SimPacket> StreamTransferFixture::receivePacket(const scl::Stream<BVec, Meta...>& stream, scl::SimulationSequencer& test, Clock clk, std::uint64_t unreadyBeatMask)
	{
		HCL_DESIGNCHECK_HINT(stream.template has<scl::Eop>(), "Every packet stream uses an EOP");
		
		SimPacket result;
		
		constexpr bool hasEmpty = stream.template has<scl::Empty>();
		constexpr bool hasReady = stream.template has<scl::Ready>();
		constexpr bool hasError = stream.template has<scl::Error>();
		constexpr bool hasTxId = stream.template has<scl::TxId>();

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
				co_await performTransferWait(stream,clk);
				if constexpr (hasTxId) 
					BOOST_TEST(simu(txid(stream)) == *result.txid());
				
			}
			needAwaitNextBeat = true;

			// fetch payload
			auto beatPayload = simu(*stream).eval();
			// Potentially crop off bytes in last beat
			if constexpr (hasEmpty) 
				if (simu(eop(stream))) {
					size_t numBytesToCrop = simu(empty(stream));
					//BOOST_TEST(numBytesToCrop < stream->size()/8);
					numBytesToCrop = std::min(numBytesToCrop, stream->size()/8 - 1);
					beatPayload.resize(stream->size() - numBytesToCrop*8);
				}
			if constexpr (hasError)
				if (simu(eop(stream))) 
					result.error(simu(error(stream)));
				
			// Append to what we already have received
			result << beatPayload;
		} while (!simu(eop(stream)));

		if constexpr (hasReady) 
			simu(ready(stream)) = '0';

		co_return result;
	}

	template<class... Meta>
	void StreamTransferFixture::simulateSendData(scl::RsPacketStream<UInt, Meta...>& stream, size_t group)
	{
		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::mt19937 rng{ std::random_device{}() };
			for (size_t i = 0; i < m_transfers;)
			{
				simu(sop(stream)) = '0';
				simu(eop(stream)) = '0';
				simu(*stream).invalidate();

				while ((rng() & 1) == 0)
					co_await AfterClk(m_clock);

				const size_t packetLen = std::min<size_t>(m_transfers - i, rng() % 5 + 1);
				for (size_t j = 0; j < packetLen; ++j)
				{
					simu(sop(stream)) = j == 0;
					simu(eop(stream)) = j == packetLen - 1;
					simu(*stream) = i + j + group * m_transfers;

					co_await scl::performTransferWait(stream, m_clock);
				}
				i += packetLen;
			}

			simu(sop(stream)) = '0';
			simu(eop(stream)) = '0';
			simu(*stream).invalidate();
		});
	}

	template<scl::StreamSignal T>
	void StreamTransferFixture::simulateRecvData(const T& stream)
	{
		auto recvClock = ClockScope::getClk();

		auto myTransfer = pinOut(transfer(stream)).setName("simulateRecvData_transfer");

		addSimulationProcess([=, this, &stream]()->SimProcess {
			std::vector<size_t> expectedValue(m_groups);
			while(true)
			{
				co_await OnClk(recvClock);

				if(simu(myTransfer) == '1')
				{
					size_t data = simu(*(stream.operator ->()));
					BOOST_TEST(data / m_transfers < expectedValue.size());
					if (data / m_transfers < expectedValue.size())
					{
						BOOST_TEST(data % m_transfers == expectedValue[data / m_transfers]);
						expectedValue[data / m_transfers]++;
					}
				}

				if (std::ranges::all_of(expectedValue, [=, this](size_t val) { return val == m_transfers; }))
				{
					stopTest();
					co_await AfterClk(recvClock);
				}
			}
		});
	}



}









