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
#include "CrcHandler.h"
#include "Descriptor.h"
#include "../../TransactionalFifo.h"
#include "../../stream/Stream.h"

namespace gtry::scl::usb
{
	struct TokenPacket
	{
		UInt address = 7_b;
		UInt endPoint = 4_b;
	};

	struct SetupPacket
	{
		UInt requestType = 8_b;
		UInt request = 8_b;
		UInt wValue = 16_b;
		UInt wIndex = 16_b;
		UInt wLength = 16_b;
	};

	class Function
	{
		enum class State
		{
			waitForToken,
			waitForSetup,
			sendSetupData,
			recvSetupData,
			sendDataPid,
			ack,

			// used to CDC debugging
			sendData,

			recvDataPid,
			recvData,
		};
	public:
		struct StreamData
		{
			UInt data = 8_b;
			UInt endPoint = 4_b;
		};

		struct RxStream
		{
			Bit ready; // valid & !ready is considered an error and will rollback
			Bit valid;
			Bit sop;
			UInt endPoint = 4_b;
			UInt data = 8_b;

			Bit eop;
			Bit error;
		};

		struct TxStream
		{
			Bit ready;
			Bit commit;
			Bit rollback;

			Bit valid;
			UInt endPoint = 4_b;
			UInt data = 8_b;
		};

	public:
		Function();

		Descriptor& descriptor() { return m_descriptor; }
		void addClassSetupHandler(std::function<Bit(const SetupPacket&)> handler);
		void addClassDataHandler(std::function<void(const BVec&)> handler);
		
		void setup(Phy& phy);

		template<class Tphy>
		Tphy& setup()
		{
			auto scope = m_area.enter();
			auto phy = std::make_unique<Tphy>();
			Tphy& phyRef = *phy;
			setup(phyRef);
			m_phyMem = move(phy);
			return phyRef;
		}

		const UInt& frameId() const { return m_frameId; }
		const UInt& deviceAddress() const { return m_address; }
		const UInt& configuration() const { return m_configuration; }
		Clock clock() const { return *m_clock; }

		RxStream& rx() { return m_rx; }
		TxStream& tx() { return m_tx; }

		void attachRxFifo(TransactionalFifo<StreamData>& fifo, uint16_t endPointMask = 0xFFFE);
		void attachTxFifo(TransactionalFifo<StreamData>& fifo, uint16_t endPointMask = 0xFFFE);

		RvStream<BVec> rxEndPointFifo(size_t endPoint, size_t fifoDpeth);
		void txEndPointFifo(size_t endPoint, size_t fifoDpeth, RvStream<BVec> data);

	protected:
		void generateCapturePacket();

		void generateInitialFsm();
		void generateHandshakeFsm();
		void generateDescriptorRom();
		void generateRxStream();
		void generateFunctionReset();

		void sendHandshake(Handshake type);

	private:
		Area m_area;

		std::unique_ptr<Phy> m_phyMem;
		CrcHandler m_phy;
		std::optional<Clock> m_clock;
		PhyRxStatus m_rxStatus;

		Descriptor m_descriptor;
		std::vector<std::function<Bit(const SetupPacket&)>> m_classHandler;
		std::vector<std::function<void(const BVec&)>> m_classDataHandler;

		Reg<Enum<State>> m_state;
		UInt m_address = 7_b;
		UInt m_frameId = 11_b;
		UInt m_endPoint = 4_b;
		UInt m_endPointMask = 16_b;
		UInt m_configuration = 4_b;

		RxStream m_rx;
		TxStream m_tx;
		Bit m_rxReadyError;

		UInt m_pid = 4_b;
		UInt m_packetData = 8 * 8_b;
		UInt m_packetLen;
		UInt m_packetLenTxLimit;
		size_t m_maxPacketSize = 8;

		UInt m_sendHandshake = 2_b;
		Bit m_rxIdle;

		UInt m_descAddress;
		UInt m_descAddressActive; // active is used to restore address on failure
		UInt m_descLength;
		UInt m_descLengthActive;
		UInt m_descData = 8_b;

		Enum<State> m_sendDataState;
		UInt m_nextOutDataPid = 16_b;
		UInt m_nextInDataPid = 16_b;

		UInt m_newAddress = 7_b;
	};

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::SetupPacket, requestType, request, wValue, wIndex, wLength);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::Function::RxStream, valid, sop, endPoint, data, eop, error);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::Function::TxStream, ready, commit, rollback, valid, endPoint, data);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::Function::StreamData, data, endPoint);
