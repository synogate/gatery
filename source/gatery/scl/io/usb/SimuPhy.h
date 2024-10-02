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
#include "Phy.h"
#include "Descriptor.h"

#ifdef interface
#undef interface
#endif

namespace gtry::scl::usb
{
	enum class SetupType
	{
		standard = 0,
		class_ = 1,
		vendor = 2,
		reserved = 3,
	};

	enum class SetupRecipient
	{
		device = 0,
		interface = 1,
		endpoint = 2,
		other = 3,
	};

	struct SimSetupPacket
	{
		EndpointDirection direction = EndpointDirection::in;
		SetupType type = SetupType::standard;
		SetupRecipient recipient = SetupRecipient::device;
		uint8_t request = uint8_t(SetupRequest::GET_DESCRIPTOR);
		uint16_t value = 0;
		uint16_t index = 0;
		uint16_t length = 0;
	};

	enum class Pid
	{
		out		= 0b0001,
		in		= 0b1001,
		sof		= 0b0101,
		setup	= 0b1101,

		data0	= 0b0011,
		data1	= 0b1011,
		data2	= 0b0111,
		mdata	= 0b1111,

		ack		= 0b0010,
		nak		= 0b1010,
		stall	= 0b1110,
		nyet	= 0b0110,
	};

	class SimuBusBase
	{
	public:
		virtual SimProcess deviceReset() = 0;
		virtual SimProcess send(std::span<const std::byte> data) = 0;
		virtual SimFunction<std::vector<std::byte>> receive(size_t timeoutCycles = 5) = 0;
	};

	class SimuPhy : public Phy, public SimuBusBase
	{
	public:
		SimuPhy(std::string pinPrefix = "simu_usb_");

		SimProcess deviceReset();
		SimProcess send(std::span<const std::byte> data);
		SimFunction<std::vector<std::byte>> receive(size_t timeoutCycles = 16);

		virtual Bit setup(OpMode mode = OpMode::FullSpeedFunction) { return '1'; }

		virtual Clock& clock() { return m_clock; }
		virtual const PhyRxStatus& status() const { return m_status; }
		virtual PhyTxStream& tx() { return m_tx; }
		virtual PhyRxStream& rx() { return m_rx; }

	protected:
		Clock m_clock;
		PhyRxStatus m_status;
		PhyTxStream m_tx;
		PhyRxStream m_rx;
	};

	class SimuHostController
	{
	public:
		SimuHostController(SimuBusBase& bus, const Descriptor& descriptor);

		uint8_t functionAddress() const { return m_functionAddress; }
		void functionAddress(uint8_t address) { m_functionAddress = address; }

		SimuBusBase& bus() { return m_bus; }

		SimProcess sendToken(Pid pid, uint16_t data);
		SimProcess sendToken(Pid pid, size_t address, size_t endPoint);
		SimProcess sendData(Pid pid, std::span<const std::byte> data);
		SimProcess sendHandshake(Pid pid);
		SimFunction<std::optional<Pid>> receivePid(size_t timeoutCycles = 16);

		SimFunction<std::vector<std::byte>> transferIn(size_t endPoint);
		SimFunction<std::vector<std::byte>> transferInBatch(size_t endPoint, size_t length);
		SimFunction<std::optional<Pid>> transferOut(size_t endPoint, std::span<const std::byte> data, Pid dataPid = Pid::data0, Pid tokenPid = Pid::out);
		SimFunction<size_t> transferOutBatch(size_t endPoint, std::span<const std::byte> data);
		SimFunction<bool> transferSetup(usb::SimSetupPacket packet);

		SimFunction<bool> controlTransferOut(usb::SimSetupPacket packet, std::span<const std::byte> data = {});
		SimFunction<std::vector<std::byte>> controlTransferIn(usb::SimSetupPacket packet);
		SimFunction<bool> controlSetAddress(uint8_t newAddress);
		SimFunction<bool> controlSetConfiguration(uint8_t configuration);

		SimFunction<std::vector<std::byte>> readDescriptor(uint16_t type, uint8_t index, uint16_t length);
		SimProcess testWindowsDeviceDiscovery();

	protected:
		void checkPacketBitErrors(std::span<const std::byte> packet);
		const usb::DescriptorEntry& descriptor(size_t type, size_t index = 0);

	private:
		Clock m_clock = ClockScope::getClk();
		SimuBusBase& m_bus;
		Descriptor m_descriptor;
		uint8_t m_functionAddress = 0;
		uint8_t m_maxPacketLength = 64;
		Pid m_nextDataPidOut[16];
	};
}
