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

	SimProcess SimuHostController::sendToken(Pid pid, uint16_t data)
	{
		uint16_t token = simu_crc5_usb_generate(data & 0xEFF);

		std::array<std::byte, 3> packet;
		packet[0] = std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4);
		packet[1] = std::byte(token);
		packet[2] = std::byte(token >> 8);
		co_await bus().send(packet);
	}

	SimProcess SimuHostController::sendToken(Pid pid, size_t address, size_t endPoint)
	{
		return sendToken(pid, uint16_t(address | endPoint << 7));
	}

	SimProcess SimuHostController::sendData(Pid pid, std::span<const std::byte> data)
	{
		std::vector<std::byte> packet;
		packet.push_back(std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4));
		packet.insert(packet.end(), data.begin(), data.end());

		size_t crc = boost::crc<16, 0x8005, 0xFFFF, 0xFFFF, true, true>(data.data(), data.size_bytes());
		packet.push_back(std::byte(crc));
		packet.push_back(std::byte(crc >> 8));

		co_await bus().send(packet);
	}

	SimProcess SimuHostController::sendHandshake(Pid pid)
	{
		std::array<std::byte, 1> packet;
		packet[0] = std::byte(pid) | std::byte((std::uint8_t(pid) ^ 0xF) << 4);
		co_await bus().send(packet);
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
	
	SimuHostController::SimuHostController(SimuBusBase& bus, const Descriptor& descriptor) : m_bus(bus), m_descriptor(descriptor) {
		if(m_descriptor.device())
			m_maxPacketLength = m_descriptor.device()->MaxPacketSize;

		for(Pid& p : m_nextDataPidOut)
			p = Pid::data0;
	}
	
	SimFunction<std::optional<Pid>> SimuHostController::receivePid(size_t timeoutCycles)
	{
		std::vector<std::byte> data = co_await bus().receive(timeoutCycles);
		HCL_ASSERT(data.size() == 1);
		checkPacketBitErrors(data);

		if (data.size() == 1)
			co_return (Pid)(uint8_t(data[0]) & 0xF);
		co_return std::nullopt;
	}
	
	SimFunction<std::vector<std::byte>> SimuHostController::transferIn(size_t endPoint)
	{
		while (true)
		{
			co_await sendToken(Pid::in, m_functionAddress, endPoint);

			std::vector<std::byte> data = co_await bus().receive();
			checkPacketBitErrors(data);

			if (data.size() >= 3)
			{
				co_await sendHandshake(Pid::ack);
				co_return std::vector<std::byte>(data.begin() + 1, data.end() - 2);
			}
			HCL_ASSERT(data.size() == 1 && (uint8_t(data[0]) & 0xF) == std::uint8_t(Pid::nak));
		}
	}
	
	void SimuHostController::checkPacketBitErrors(std::span<const std::byte> packet)
	{
		HCL_ASSERT((packet.size() == 1 || packet.size() >= 3));

		if (packet.size() >= 1)
		{
			uint8_t pid = uint8_t(packet[0]) & 0xF;
			uint8_t pidCheck = uint8_t(packet[0]) >> 4;
			HCL_ASSERT(pid == (pidCheck ^ 0xF));

			if (packet.size() == 1)
				HCL_ASSERT(((pid == uint8_t(Pid::nak)) | (pid == uint8_t(Pid::ack))))
			else
				// TODO: check which one is expected
				HCL_ASSERT((pid == uint8_t(Pid::data0) || pid == uint8_t(Pid::data1)))
		}

		if (packet.size() >= 3)
		{
			switch (uint8_t(packet[0]) & 0x3)
			{
			case 0b01: // token
				HCL_ASSERT(packet.size() == 3);
				HCL_ASSERT(simu_crc5_usb_verify(uint16_t(packet[1]) | (uint16_t(packet[2]) << 8)));
				break;
			case 0b11: // data
			{
				std::span<const std::byte> checkedData = packet.subspan(1);
				size_t crcCheck = boost::crc<16, 0x8005, 0xFFFF, 0xFFFF, true, true>(checkedData.data(), checkedData.size());
				HCL_ASSERT(crcCheck == 0x4FFE);
				break;
			}
			default:;
			}
		}
	}
	
	SimFunction<std::vector<std::byte>> SimuHostController::transferInBatch(size_t endPoint, size_t length)
	{
		std::vector<std::byte> ret;
		while (true)
		{
			std::vector<std::byte> packet = co_await transferIn(endPoint);
			ret.insert(ret.end(), packet.begin(), packet.end());

			if (packet.size() != m_maxPacketLength || ret.size() >= length)
				co_return ret;
		}
	}
	
	SimFunction<std::optional<Pid>> SimuHostController::transferOut(size_t endPoint, std::span<const std::byte> data, Pid dataPid, Pid tokenPid)
	{
		co_await sendToken(tokenPid, m_functionAddress, endPoint);
		co_await sendData(dataPid, data);
		co_return co_await receivePid();
	}
	
	SimFunction<size_t> SimuHostController::transferOutBatch(size_t endPoint, std::span<const std::byte> data)
	{
		HCL_ASSERT(endPoint < 16);

		size_t sent = 0;
		while (sent < data.size())
		{
			std::span<const std::byte> packet = data.subspan(sent, std::min<size_t>(data.size() - sent, m_maxPacketLength));
			std::optional<Pid> pid = co_await transferOut(endPoint, packet, m_nextDataPidOut[endPoint]);
			if (!pid || *pid == Pid::stall)
				break;
			if (*pid == Pid::ack)
			{
				sent += packet.size();
				m_nextDataPidOut[endPoint] = m_nextDataPidOut[endPoint] == Pid::data0 ? Pid::data1 : Pid::data0;
			}
			else
				HCL_ASSERT((*pid == Pid::nak));
		}
		co_return sent;
	}
	
	SimFunction<bool> SimuHostController::transferSetup(usb::SimSetupPacket packet)
	{
		std::array<std::uint8_t, 8> setupPacket;
		setupPacket[0] = uint8_t(packet.direction) << 7 | uint8_t(packet.type) << 5 | uint8_t(packet.recipient);
		setupPacket[1] = uint8_t(packet.request);
		setupPacket[2] = uint8_t(packet.value);
		setupPacket[3] = uint8_t(packet.value >> 8);
		setupPacket[4] = uint8_t(packet.index);
		setupPacket[5] = uint8_t(packet.index >> 8);
		setupPacket[6] = uint8_t(packet.length);
		setupPacket[7] = uint8_t(packet.length >> 8);

		std::optional<Pid> pid = co_await transferOut(0, std::as_bytes(std::span(setupPacket)), Pid::data0, Pid::setup);
		HCL_ASSERT(pid.has_value());
		if (pid)
		{
			HCL_ASSERT((*pid == Pid::ack));
			co_return *pid == Pid::ack;
		}
		co_return false;
	}
	
	SimFunction<bool> SimuHostController::controlTransferOut(usb::SimSetupPacket packet, std::span<const std::byte> data)
	{
		HCL_DESIGNCHECK(data.size_bytes() == packet.length);
		HCL_DESIGNCHECK_HINT(packet.length <= 64, "no impl");

		co_await transferSetup(packet);

		// optional transfer stage
		if (packet.length != 0)
		{
			std::optional<Pid> pid = co_await transferOut(0, data);
			HCL_DESIGNCHECK(pid.has_value()); // timeout should trigger retry
			if (pid)
			{
				HCL_DESIGNCHECK(*pid != Pid::nak); // should trigger retry
				HCL_ASSERT((*pid == Pid::ack));
			}
			if (!pid || *pid != Pid::ack)
				co_return false;
		}

		std::vector<std::byte> status = co_await transferIn(0);
		HCL_ASSERT(status.empty());
		co_return status.empty();
	}
	
	SimFunction<std::vector<std::byte>> SimuHostController::controlTransferIn(usb::SimSetupPacket packet)
	{
		co_await transferSetup(packet);

		std::vector<std::byte> data;
		if (packet.length != 0)
		{
			data = co_await transferInBatch(0, packet.length);
			HCL_ASSERT(data.size() <= packet.length);
		}

		std::optional<Pid> pid = co_await transferOut(0, {});
		HCL_ASSERT(pid.has_value());
		if (pid)
		{
			HCL_ASSERT((*pid == Pid::ack));
			if (*pid == Pid::ack)
				co_return data;
		}
		co_return std::vector<std::byte>{};
	}
	
	SimFunction<bool> SimuHostController::controlSetAddress(uint8_t newAddress)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "set address");

		bool success = co_await controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = uint8_t(usb::SetupRequest::SET_ADDRESS),
			.value = newAddress
			});

		if (success)
			m_functionAddress = newAddress;

		co_return success;
	}
	
	SimFunction<bool> SimuHostController::controlSetConfiguration(uint8_t configuration)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "set configuration");

		return controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = uint8_t(usb::SetupRequest::SET_CONFIGURATION),
			.value = configuration
			});
	}
	
	SimFunction<std::vector<std::byte>> SimuHostController::readDescriptor(uint16_t type, uint8_t index, uint16_t length)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "read descriptor " + std::to_string(type));

		std::vector<std::byte> data = co_await controlTransferIn({
			.value = uint16_t(type << 8 | index),
			.length = length,
			});

		HCL_ASSERT(data.size() >= 2);
		if (data.size() >= 2)
		{
			HCL_ASSERT(uint8_t(data[1]) == type);

			bool firstDescFound = false;
			std::span<const std::byte> checkRange = data;
			for (const DescriptorEntry& d : m_descriptor.entries())
			{
				if (firstDescFound || index == d.index && type == d.type())
				{
					firstDescFound = true;

					size_t checkLen = std::min(checkRange.size(), d.data.size());
					HCL_ASSERT(!memcmp(d.data.data(), checkRange.data(), checkLen));
					checkRange = checkRange.subspan(checkLen);

					if (checkRange.empty() || type != ConfigurationDescriptor::TYPE)
						break;
				}
			}
		}

		co_return data;
	}
	
	SimProcess SimuHostController::testWindowsDeviceDiscovery()
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "ask for the first 64b of the descriptor");
		co_await transferSetup({ .value = uint16_t(DeviceDescriptor::TYPE) << 8, .length = 64 });

		auto checkDevDescriptor = [&](const std::vector<std::byte>& data) {
			HCL_ASSERT(data.size() == std::min<size_t>(sizeof(usb::DeviceDescriptor) + 2, m_maxPacketLength));

			if (data.size() >= 2)
			{
				HCL_ASSERT(uint8_t(data[0]) == sizeof(usb::DeviceDescriptor) + 2);
				HCL_ASSERT(uint8_t(data[1]) == usb::DeviceDescriptor::TYPE);

				if (m_descriptor.device() && data.size() >= sizeof(usb::DeviceDescriptor) + 2)
					HCL_ASSERT(!memcmp(data.data() + 2, m_descriptor.device(), sizeof(usb::DeviceDescriptor)))
			}
		};
		checkDevDescriptor(co_await transferIn(0));

		sim::SimulationContext::current()->onDebugMessage(nullptr, "reset device");
		co_await bus().deviceReset();

		co_await controlSetAddress(5);

		co_await readDescriptor(DeviceDescriptor::TYPE, 0, 18);
		std::vector<std::byte> confDescPrefix = co_await readDescriptor(ConfigurationDescriptor::TYPE, 0, 9);
		HCL_ASSERT(confDescPrefix.size() == 9);

		std::vector<std::byte> confDesc = co_await readDescriptor(ConfigurationDescriptor::TYPE, 0, 255);
		HCL_ASSERT(confDesc.size() >= 9);
		HCL_ASSERT(!memcmp(confDesc.data(), confDescPrefix.data(), confDescPrefix.size()));

		const DescriptorEntry& confDescEntry = descriptor(ConfigurationDescriptor::TYPE);
		const size_t confDescSize = confDescEntry.data[2] | (confDescEntry.data[3] << 8);
		HCL_ASSERT(confDesc.size() == confDescSize);

		co_await controlSetConfiguration(1);
	}
	
	const usb::DescriptorEntry& SimuHostController::descriptor(size_t type, size_t index)
	{
		for (const usb::DescriptorEntry& d : m_descriptor.entries())
		{
			if (d.type() == type && d.index == index)
				return d;
		}
		throw std::runtime_error("descriptor not found");
	}
}
