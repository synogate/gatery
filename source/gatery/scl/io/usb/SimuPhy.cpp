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
#include "gatery/scl_pch.h"
#include "SimuPhy.h"

#include "../../crc.h"

#include <boost/crc.hpp>

namespace gtry::scl::usb
{
	SimuPhy::SimuPhy(std::string pinPrefix) :
		m_clock(ClockScope::getClk())
	{
		const PinNodeParameter simPin{ .simulationOnlyPin = true };

		pinIn(m_status, pinPrefix + "status", simPin);
		pinIn(m_rx, pinPrefix + "rx", simPin);

		pinIn(m_tx.ready, pinPrefix + "tx_ready", simPin);
		pinOut(m_tx.valid, pinPrefix + "tx_valid", simPin);
		pinOut(m_tx.error, pinPrefix + "tx_error", simPin);
		pinOut(m_tx.data, pinPrefix + "tx_data", simPin);

		DesignScope::get()->getCircuit().addSimulationProcess([=]() -> SimProcess {
			simu(m_status.lineState) = 1;
			simu(m_status.sessEnd) = '0';
			simu(m_status.sessValid) = '0';
			simu(m_status.vbusValid) = '1';
			simu(m_status.rxActive) = '0';
			simu(m_status.rxError) = '0';
			simu(m_status.hostDisconnect) = '0';
			simu(m_status.id) = '0';
			simu(m_status.altInt) = '0';

			simu(m_rx.valid) = '0';
			simu(m_rx.eop) = '0';
			simu(m_rx.error) = '0';

			simu(m_tx.ready) = '1';

			co_return;
		});
	}

	SimProcess SimuPhy::deviceReset()
	{
		simu(m_status.lineState) = 0;
		simu(m_status.sessEnd) = '1';
		
		// our function interprets sessEnd as reset signal, so we dont have to wait for 10ms
		for (size_t i = 0; i < 8; ++i)
			co_await OnClk(m_clock);

		simu(m_status.lineState) = 1;
		simu(m_status.sessEnd) = '0';
	}

	SimProcess SimuPhy::send(std::span<const std::byte> data)
	{
		simu(m_status.rxActive) = '1';
		co_await OnClk(m_clock);

		simu(m_rx.valid) = '1';
		simu(m_rx.sop) = '1';
		for (std::byte b : data)
		{
			simu(m_rx.data) = uint8_t(b);
			co_await OnClk(m_clock);
			simu(m_rx.sop) = '0';
		}

		simu(m_rx.valid) = '0';
		simu(m_rx.sop).invalidate();
		simu(m_rx.data).invalidate();

		co_await OnClk(m_clock);
		co_await OnClk(m_clock);

		simu(m_rx.eop) = '1';
		//simu(m_rx.error) = '0';
		co_await OnClk(m_clock);
		simu(m_rx.eop) = '0';
		//simu(m_rx.error).invalidate();

		simu(m_status.rxActive) = '0';
	}

	SimProcess SimuHostBase::sendToken(Pid pid, uint16_t data)
	{
		uint16_t token = simu_crc5_usb_generate(data & 0xEFF);

		std::array<std::byte, 3> packet;
		packet[0] = std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4);
		packet[1] = std::byte(token);
		packet[2] = std::byte(token >> 8);
		co_await send(packet);
	}

	SimProcess SimuHostBase::sendToken(Pid pid, size_t address, size_t endPoint)
	{
		return sendToken(pid, uint16_t(address | endPoint << 7));
	}

	SimProcess SimuHostBase::sendData(Pid pid, std::span<const std::byte> data)
	{
		std::vector<std::byte> packet;
		packet.push_back(std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4));
		packet.insert(packet.end(), data.begin(), data.end());

		size_t crc = boost::crc<16, 0x8005, 0xFFFF, 0xFFFF, true, true>(data.data(), data.size_bytes());
		packet.push_back(std::byte(crc));
		packet.push_back(std::byte(crc >> 8));

		co_await send(packet);
	}

	SimProcess SimuHostBase::sendHandshake(Pid pid)
	{
		std::array<std::byte, 1> packet;
		packet[0] = std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4);
		co_await send(packet);
	}

	SimFunction<std::vector<std::byte>> SimuPhy::receive(size_t timeoutCycles)
	{
		for (size_t i = 0; i < timeoutCycles; ++i)
		{
			if (simu(m_tx.valid) == '1')
			{
				std::vector<std::byte> data;
				while (simu(m_tx.valid) == '1')
				{
					data.push_back((std::byte)std::uint8_t(simu(m_tx.data)));
					co_await OnClk(m_clock);
				}
				co_return data;
			}
			co_await OnClk(m_clock);
		}
		co_return std::vector<std::byte>{};
	}
}
