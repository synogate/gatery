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
		SetupRequest request = SetupRequest::GET_DESCRIPTOR;
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

	class SimuHostBase
	{
	public:
		virtual SimProcess deviceReset() = 0;
		virtual SimProcess send(std::span<const std::byte> data) = 0;
		virtual SimFunction<std::vector<std::byte>> receive(size_t timeoutCycles = 5) = 0;

		SimProcess sendToken(Pid pid, uint16_t data);
		SimProcess sendToken(Pid pid, size_t address, size_t endPoint);
		SimProcess sendData(Pid pid, std::span<const std::byte> data);
		SimProcess sendHandshake(Pid pid);
	};

	class SimuPhy : public Phy, public SimuHostBase
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
		SimuHostController(SimuHostBase& host) : m_host(host) {}

		void functionAddress(uint8_t address) { m_functionAddress = address; }

		SimFunction<std::optional<Pid>> receivePid(size_t timeoutCycles = 16);
		SimFunction<bool> transferSetup(SimSetupPacket packet);
		SimFunction<std::vector<std::byte>> transferIn(size_t endPoint);

	private:
		Clock m_clock = ClockScope::getClk();
		SimuHostBase& m_host;

		uint8_t m_functionAddress = 0;
		std::mt19937 m_rng;
	};
}
