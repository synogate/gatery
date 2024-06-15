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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/crc.h>
#include <gatery/scl/io/usb/SimuPhy.h>
#include <gatery/scl/io/usb/Function.h>
#include <gatery/scl/io/usb/GpioPhy.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;
using namespace gtry::scl::usb;

class UsbFixture : public BoostUnitTestSimulationFixture
{
public:

	virtual void setupFunction()
	{
		m_func.emplace();
		setupDescriptor(*m_func);
		setupPhy(*m_func);
		pin(*m_func);
	}
	
	virtual void setupDescriptor(usb::Function& func)
	{
		usb::Descriptor& desc = func.descriptor();

		desc.add(scl::usb::DeviceDescriptor{
			.Class = scl::usb::InterfaceAssociationDescriptor::DevClass,
			.SubClass = scl::usb::InterfaceAssociationDescriptor::DevSubClass,
			.Protocol = scl::usb::InterfaceAssociationDescriptor::DevProtocol,
			.ManufacturerName = desc.allocateStringIndex(L"Gatery"),
			.ProductName = desc.allocateStringIndex(L"MultiCom")
			});

		desc.add(scl::usb::ConfigurationDescriptor{});
		desc.add(scl::usb::InterfaceAssociationDescriptor{});
		scl::usb::virtualCOMsetup(func, 0, 1);
		desc.add(scl::usb::InterfaceAssociationDescriptor{});
		scl::usb::virtualCOMsetup(func, 1, 2);

		desc.changeMaxPacketSize(m_maxPacketLength);
		desc.finalize();
	}

	virtual void setupPhy(usb::Function& func)
	{
		if (m_useSimuPhy)
			m_host = &func.setup<usb::SimuPhy>();
		else
			m_host = &func.setup<usb::GpioPhy>();
	}

	virtual void pin(usb::Function& func)
	{
		PinNodeParameter simpin{ .simulationOnlyPin = true };

		if(m_pinStatusRegister)
		{
			pinOut(func.frameId(), "frameId", simpin);
			pinOut(func.deviceAddress(), "deviceAddress", simpin);
			pinOut(func.configuration(), "configuration", simpin);
		}

		if (m_pinApplicationInterface)
		{
			func.rx().ready = '1';
			pinOut(func.rx(), "rx", simpin);

			pinOut(func.tx().ready, "tx_ready", simpin);
			pinOut(func.tx().commit, "tx_commit", simpin);
			pinOut(func.tx().rollback, "tx_rollback", simpin);
			pinIn(func.tx().valid, "tx_valid", simpin);
			pinIn(func.tx().endPoint, "tx_endPoint", simpin);
			pinIn(func.tx().data, "tx_data", simpin);

			addSimulationProcess([&]() -> SimProcess {
				simu(func.tx().valid) = '0';
				co_await OnClk(func.clock());
			});
		}
	}

	SimFunction<std::optional<Pid>> receivePid(size_t timeoutCycles = 16)
	{
		std::vector<std::byte> data = co_await m_host->receive(timeoutCycles);
		BOOST_TEST(data.size() == 1);

		if (data.size() == 1)
		{
			uint8_t pid = uint8_t(data[0]) & 0xF;
			uint8_t pidCheck = uint8_t(data[0]) >> 4;
			BOOST_TEST(pid == (pidCheck ^ 0xF));
			co_return (Pid)(pid);
		}
		co_return std::nullopt;
	}

	SimFunction<std::vector<std::byte>> transferIn(size_t endPoint)
	{
		co_await m_host->sendToken(Pid::in, m_functionAddress, endPoint);

		std::vector<std::byte> data = co_await m_host->receive();
		BOOST_TEST((data.size() == 1 || data.size() >= 3));

		if (data.size() >= 1)
		{
			uint8_t pid = uint8_t(data[0]) & 0xF;
			uint8_t pidCheck = uint8_t(data[0]) >> 4;
			BOOST_TEST(pid == (pidCheck ^ 0xF));

			if (data.size() == 1)
				BOOST_TEST(pid == uint8_t(Pid::nak));
			else
				// TODO: check which one is expected
				BOOST_TEST((pid == uint8_t(Pid::data0) || pid == uint8_t(Pid::data1)));
		}

		if(data.size() >= 3)
		{
			co_await m_host->sendHandshake(Pid::ack);
			co_return std::vector<std::byte>(data.begin() + 1, data.end() - 2);
		}

		co_return std::vector<std::byte>{};
	}

	SimFunction<std::vector<std::byte>> transferInBatch(size_t endPoint, size_t length)
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

	SimFunction<std::optional<Pid>> transferOut(size_t endPoint, std::span<const std::byte> data, Pid dataPid = Pid::data0, Pid tokenPid = Pid::out)
	{
		co_await m_host->sendToken(tokenPid, m_functionAddress, endPoint);
		co_await m_host->sendData(dataPid, data);
		co_return co_await receivePid();
	}

	SimFunction<size_t> transferOutBatch(size_t endPoint, std::span<const std::byte> data)
	{
		Pid dataPid = Pid::data0;
		size_t sent = 0;
		while (sent < data.size())
		{
			std::span<const std::byte> packet = data.subspan(sent, std::min<size_t>(data.size() - sent, m_maxPacketLength));
			std::optional<Pid> pid = co_await transferOut(endPoint, packet, dataPid);
			if (!pid || *pid == Pid::stall)
				break;
			if(*pid == Pid::ack)
			{
				sent += packet.size();
				dataPid = dataPid == Pid::data0 ? Pid::data1 : Pid::data0;
			}
			else
				BOOST_TEST((*pid == Pid::nak));
		}
		co_return sent;
	}

	SimFunction<bool> transferSetup(usb::SimSetupPacket packet)
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
		BOOST_TEST(pid.has_value());
		if (pid)
		{
			BOOST_TEST((*pid == Pid::ack));
			co_return *pid == Pid::ack;
		}
		co_return false;
	}

	SimFunction<bool> controlTransferOut(usb::SimSetupPacket packet, std::span<const std::byte> data = {})
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
				BOOST_TEST((*pid == Pid::ack));
			}
			if (!pid || *pid != Pid::ack)
				co_return false;
		}

		std::vector<std::byte> status = co_await transferIn(0);
		BOOST_TEST(status.empty());
		co_return status.empty();
	}

	SimFunction<std::vector<std::byte>> controlTransferIn(usb::SimSetupPacket packet)
	{
		co_await transferSetup(packet);

		std::vector<std::byte> data;
		if (packet.length != 0)
		{
			data = co_await transferInBatch(0, packet.length);
			BOOST_TEST(data.size() <= packet.length);
		}

		std::optional<Pid> pid = co_await transferOut(0, {});
		BOOST_TEST(pid.has_value());
		if (pid)
		{
			BOOST_TEST((*pid == Pid::ack));
			if (*pid == Pid::ack)
				co_return data;
		}
		co_return std::vector<std::byte>{};
	}

	SimFunction<bool> controlSetAddress(uint8_t newAddress)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "set address");

		bool success = co_await controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = usb::SetupRequest::SET_ADDRESS,
			.value = newAddress
		});

		if (success)
			m_functionAddress = newAddress;

		co_return success;
	}

	SimFunction<bool> controlSetConfiguration(uint8_t configuration)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "set configuration");

		return controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = usb::SetupRequest::SET_CONFIGURATION,
			.value = configuration
		});
	}

	SimFunction<std::vector<std::byte>> readDescriptor(uint16_t type, uint8_t index, uint16_t length)
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "read descriptor " + std::to_string(type));

		std::vector<std::byte> data = co_await controlTransferIn({ 
			.value = uint16_t(type << 8 | index), 
			.length = length,
		});

		BOOST_TEST(data.size() >= 2);
		if (data.size() >= 2)
		{
			BOOST_TEST(uint8_t(data[1]) == type);
			
			bool firstDescFound = false;
			std::span<const std::byte> checkRange = data;
			for (const DescriptorEntry& d : m_func->descriptor().entries())
			{
				if (firstDescFound || index == d.index && type == d.type())
				{
					firstDescFound = true;

					size_t checkLen = std::min(checkRange.size(), d.data.size());
					BOOST_TEST(!memcmp(d.data.data(), checkRange.data(), checkLen));
					checkRange = checkRange.subspan(checkLen);

					if (checkRange.empty() || type != ConfigurationDescriptor::TYPE)
						break;
				}
			}
		}

		co_return data;
	}

	SimProcess testWindowsDeviceDiscovery()
	{
		sim::SimulationContext::current()->onDebugMessage(nullptr, "ask for the first 64b of the descriptor");
		co_await transferSetup({ .value = uint16_t(DeviceDescriptor::TYPE) << 8, .length = 64 });

		auto checkDevDescriptor = [&](const std::vector<std::byte>& data) {
			BOOST_TEST(data.size() == std::min<size_t>(sizeof(usb::DeviceDescriptor) + 2, m_maxPacketLength));

			if (data.size() >= 2)
			{
				BOOST_TEST(uint8_t(data[0]) == sizeof(usb::DeviceDescriptor) + 2);
				BOOST_TEST(uint8_t(data[1]) == usb::DeviceDescriptor::TYPE);

				if (data.size() >= sizeof(usb::DeviceDescriptor) + 2)
					BOOST_TEST(!memcmp(data.data() + 2, m_func->descriptor().device(), sizeof(usb::DeviceDescriptor)));
			}
			};
		checkDevDescriptor(co_await transferIn(0));

		sim::SimulationContext::current()->onDebugMessage(nullptr, "reset device");
		co_await m_host->deviceReset();
		if (m_pinStatusRegister)
		{
			BOOST_TEST(simu(m_func->deviceAddress()) == 0);
			BOOST_TEST(simu(m_func->configuration()) == 0);
		}

		co_await controlSetAddress(5);

		co_await readDescriptor(DeviceDescriptor::TYPE, 0, 18);
		std::vector<std::byte> confDescPrefix = co_await readDescriptor(ConfigurationDescriptor::TYPE, 0, 9);
		BOOST_TEST(confDescPrefix.size() == 9);

		std::vector<std::byte> confDesc = co_await readDescriptor(ConfigurationDescriptor::TYPE, 0, 255);
		BOOST_TEST(confDesc.size() >= 9);
		BOOST_TEST(!memcmp(confDesc.data(), confDescPrefix.data(), confDescPrefix.size()));

		const DescriptorEntry& confDescEntry = descriptor(ConfigurationDescriptor::TYPE);
		const size_t confDescSize = confDescEntry.data[2] | (confDescEntry.data[3] << 8);
		BOOST_TEST(confDesc.size() == confDescSize);

		co_await controlSetConfiguration(1);

		if (m_pinStatusRegister)
		{
			BOOST_TEST(simu(m_func->configuration()) == 1);
			BOOST_TEST(simu(m_func->deviceAddress()) == m_functionAddress);
		}
	}

	const usb::DescriptorEntry& descriptor(size_t type, size_t index = 0)
	{ 
		for (const usb::DescriptorEntry& d : m_func->descriptor().entries())
		{
			if (d.type() == type && d.index == index)
				return d;
		}
		throw std::runtime_error("descriptor not found");
	}

protected:
	bool m_useSimuPhy = true;
	bool m_pinApplicationInterface = true;
	bool m_pinStatusRegister = true;

	std::optional<usb::Function> m_func;
	usb::SimuHostBase* m_host = nullptr;

	uint8_t m_functionAddress = 0;
	uint8_t m_maxPacketLength = 64;
};


BOOST_FIXTURE_TEST_CASE(usb_windows_discovery, UsbFixture)
{
	// uncomment to enable full gpiophy simulation
	//m_useSimuPhy = false;

	Clock clock({ .absoluteFrequency = 12'000'000 * (m_useSimuPhy ? 1 : 4) });
	ClockScope clkScp(clock);

	setupFunction();

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);

		co_await m_host->sendToken(usb::Pid::sof, 0x2CD);
		co_await testWindowsDeviceDiscovery();
		BOOST_TEST(simu(m_func->frameId()) == 0x2CD);

		// send data
		sim::SimulationContext::current()->onDebugMessage(nullptr, "transfer out");
		const char testString[] = "Hello World!!!";
		std::optional<Pid> pid = co_await transferOut(1, std::as_bytes(std::span(testString)));
		BOOST_TEST((pid && *pid == Pid::ack));

		// receive nothing
		std::vector<std::byte> dataIn = co_await transferIn(1);
		BOOST_TEST(dataIn.empty());

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

class SingleEndpointUsbFixture : public UsbFixture
{
public:
	virtual void setupDescriptor(usb::Function& func)
	{
		// this is a minimal descriptor for a single cdc com port
		usb::Descriptor& desc = func.descriptor();
		desc.add(scl::usb::DeviceDescriptor{.Class = scl::usb::ClassCode::Communications_and_CDC_Control});
		desc.add(scl::usb::ConfigurationDescriptor{});
		scl::usb::virtualCOMsetup(func, 0, 1, 2);

		desc.changeMaxPacketSize(m_maxPacketLength);
		desc.finalize();
	}
};

BOOST_FIXTURE_TEST_CASE(usb_loopback_cyc10, SingleEndpointUsbFixture, *boost::unit_test::disabled())
{
	m_useSimuPhy = false;
	m_pinApplicationInterface = false;
	m_pinStatusRegister = false;
	m_maxPacketLength = 8;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupDevice("10CL025YU256C8G");
	design.setTargetTechnology(std::move(device));

	Clock clk12{ ClockConfig{
		.absoluteFrequency = 12'000'000,
		.name = "CLK12M",
		.resetType = ClockConfig::ResetType::NONE
	} };

	auto* pll2 = DesignScope::get()->createNode<scl::arch::intel::ALTPLL>();
	pll2->setClock(0, clk12);
	Clock clock = pll2->generateOutClock(0, 4, 1, 50, 0);
	ClockScope clkScp(clock);

	setupFunction();
	{
		scl::TransactionalFifo<scl::usb::Function::StreamData> loopbackFifo{ 256 };
		//m_func->attachRxFifo(loopbackFifo, 1 << 1);
		m_func->rx().ready = '1';
		m_func->attachTxFifo(loopbackFifo, 1 << 1);

		IF(scl::Counter(64).isLast() & !loopbackFifo.full())
			loopbackFifo.push({ .data = scl::Counter(8_b).value(), .endPoint = 1});

		loopbackFifo.generate();
	}

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);
		//co_await testWindowsDeviceDiscovery();
		co_await controlSetConfiguration(1);

		co_await WaitFor({ 20, 1'000'000 });

		//std::vector<std::byte> dataFirstIn = co_await transferInBatch(1, 255);
		//std::cout << "dataFirstIn " << dataFirstIn.size() << "\n";

		// receive a packet but do not ack
		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 1");
		co_await m_host->sendToken(Pid::in, m_functionAddress, 1);
		std::vector<std::byte> data = co_await m_host->receive();

		sim::SimulationContext::current()->onDebugMessage(nullptr, "control");
		co_await controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = usb::SetupRequest::CLEAR_FEATURE,
			.index = 0x81
		});

		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 2");
		std::vector<std::byte> data2 = co_await transferIn(1);

		stopTest();
	});

	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/usb_loopback_cyc10/usb_loopback_cyc10.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_resend_setup_interrupted, UsbFixture, *boost::unit_test::disabled())
{
	m_pinApplicationInterface = false;
	m_pinStatusRegister = false;

	Clock clock({ .absoluteFrequency = 12'000'000 * (m_useSimuPhy ? 1 : 4) });
	ClockScope clkScp(clock);

	setupFunction();
	{
		scl::TransactionalFifo<scl::usb::Function::StreamData> loopbackFifo{ 256 };
		//m_func->attachRxFifo(loopbackFifo, 1 << 1);
		m_func->rx().ready = '1';
		m_func->attachTxFifo(loopbackFifo, 1 << 1);

		scl::Counter ctr{ 256 };
		scl::Counter character{ 256 };
		IF(ctr.isLast() & !loopbackFifo.full())
		{
			loopbackFifo.push({ .data = zext(character.value(), 8_b), .endPoint = 1 });
			character.inc();
		}

		loopbackFifo.generate();
	}

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);
		co_await controlSetConfiguration(1);

		co_await WaitFor({ 20, 1'000'000 });

		// receive a packet but do not ack
		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 1");
		co_await m_host->sendToken(Pid::in, m_functionAddress, 1);
		std::vector<std::byte> data1 = co_await m_host->receive();
		BOOST_TEST(data1.size() > 3);

		// interrupt by control transfer which changes the endpoint
		sim::SimulationContext::current()->onDebugMessage(nullptr, "control");
		co_await controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = usb::SetupRequest::CLEAR_FEATURE,
			.index = 0x81
		});

		co_await WaitFor({ 20, 1'000'000 });

		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 2");
		co_await m_host->sendToken(Pid::in, m_functionAddress, 1);
		std::vector<std::byte> data2 = co_await m_host->receive();
		BOOST_TEST(data1 == data2);

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_phy_gpio_fuzz, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 12'000'000 * 4 });
	ClockScope clkScp(clock);

	GpioPhy phy1;
	phy1.setup();
	phy1.tx().valid = '0';
	phy1.tx().error = '0';

	pinOut(phy1.rx(), "rx");

	std::queue<std::vector<std::byte>> packets;

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);

		std::mt19937 rng{ 220620 };
		for (size_t i = 0;; ++i)
		{
			std::vector<std::byte> packet(rng() % 70 + 1);
			for (std::byte& b : packet)
				b = std::byte(rng());
			packets.push(packet);
			co_await phy1.send(packet, {1, 12'000'000 - 120'000});
		}
	});

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);

		for (size_t i = 0; i < 4; ++i)
		{
			std::vector<std::byte> packet;
			while(true) 
			{
				if (simu(phy1.rx().valid) == '1')
					packet.push_back(std::byte(simu(phy1.rx().data).value()));
				if (simu(phy1.rx().eop) == '1')
					break;
				co_await OnClk(clock);
			}
			BOOST_TEST((!packets.empty() && packet == packets.front()));
			packets.pop();
			co_await OnClk(clock);
		}

		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1 }));
}
