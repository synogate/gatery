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
#include <boost/crc.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/crc.h>
#include <gatery/scl/io/usb/SimuPhy.h>
#include <gatery/scl/io/usb/Function.h>
#include <gatery/scl/io/usb/GpioPhy.h>
#include <gatery/scl/io/uart.h>
#include <gatery/scl/io/BitBangEngine.h>

#include <gatery/scl/io/dynamicDelay.h>
#include <gatery/scl/arch/sky130/Sky130Device.h>
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
		for (const auto& handler : m_setupCallback)
			handler(*m_func);

		setupDescriptor(*m_func);
		setupPhy(*m_func);
		pin(*m_func);

		m_controller.emplace(*m_host, m_func->descriptor());
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

protected:
	bool m_useSimuPhy = true;
	bool m_pinApplicationInterface = true;
	bool m_pinStatusRegister = true;
	uint8_t m_maxPacketLength = 64;
	std::list<std::function<void(Function&)>> m_setupCallback;

	std::optional<usb::Function> m_func;
	usb::SimuBusBase* m_host = nullptr;
	std::optional<usb::SimuHostController> m_controller;
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

		co_await m_controller->sendToken(usb::Pid::sof, 0x2CD);
		co_await m_controller->testWindowsDeviceDiscovery();
		BOOST_TEST(simu(m_func->frameId()) == 0x2CD);

		// send data
		sim::SimulationContext::current()->onDebugMessage(nullptr, "transfer out");
		const char testString[] = "Hello World!!!";
		std::optional<Pid> pid = co_await m_controller->transferOut(1, std::as_bytes(std::span(testString)));
		BOOST_TEST((pid && *pid == Pid::ack));

		// receive nothing
		co_await m_controller->sendToken(Pid::in, m_controller->functionAddress(), 1);
		std::vector<std::byte> data = co_await m_controller->bus().receive();
		BOOST_TEST((data.size() == 1 && data[0] == std::byte(0x5A)));

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
		co_await m_controller->controlSetConfiguration(1);

		co_await WaitFor({ 20, 1'000'000 });

		//std::vector<std::byte> dataFirstIn = co_await transferInBatch(1, 255);
		//std::cout << "dataFirstIn " << dataFirstIn.size() << "\n";

		// receive a packet but do not ack
		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 1");
		co_await m_controller->sendToken(Pid::in, m_controller->functionAddress(), 1);
		std::vector<std::byte> data = co_await m_host->receive();

		sim::SimulationContext::current()->onDebugMessage(nullptr, "control");
		co_await m_controller->controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = uint8_t(usb::SetupRequest::CLEAR_FEATURE),
			.index = 0x81
		});

		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 2");
		std::vector<std::byte> data2 = co_await m_controller->transferIn(1);

		stopTest();
	});

	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/usb_loopback_cyc10/usb_loopback_cyc10.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

class WindowsDiscoveryLoopbackUsbFixture : public SingleEndpointUsbFixture {
public:
	size_t samplingRatio = 4;
	std::function<void()> configure = []() {};
	void runTest() {
		configure();
		m_useSimuPhy = false;
		m_pinApplicationInterface = false;
		m_pinStatusRegister = false;
		m_maxPacketLength = 64;

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
		Clock clock = pll2->generateOutClock(0, samplingRatio, 1, 50, 0);
		ClockScope clkScp(clock);

		setupFunction();
		{
			scl::TransactionalFifo<scl::usb::Function::StreamData> loopbackFifo{ 256 };
			m_func->attachRxFifo(loopbackFifo, 1 << 1);
			m_func->attachTxFifo(loopbackFifo, 1 << 1);
			loopbackFifo.generate();
		}

		addSimulationProcess([&]() -> SimProcess {
			co_await OnClk(clock);
			co_await m_controller->testWindowsDeviceDiscovery();
			stopTest();
			});

		design.postprocess();

		vhdl::VHDLExport vhdl("synthesis_projects/usb_loopback_cyc10/usb_loopback_cyc10.vhd");
		vhdl.targetSynthesisTool(new IntelQuartus());
		vhdl(design.getCircuit());

		BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(usb_loopback_cyc10_oversampled, WindowsDiscoveryLoopbackUsbFixture, *boost::unit_test::disabled())
{
	samplingRatio = 4;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(usb_loopback_cyc10_dirty, WindowsDiscoveryLoopbackUsbFixture, *boost::unit_test::disabled())
{
	samplingRatio = 1;
	configure = []() { hlim::NodeGroup::configTree("scl_recoverDataDifferential*", "version", "dirty"); };
	runTest();
}

BOOST_FIXTURE_TEST_CASE(usb_loopback_cyc10_clean, WindowsDiscoveryLoopbackUsbFixture, *boost::unit_test::disabled())
{
	samplingRatio = 1;
	runTest();
}

class WindowsDiscoveryLoopbackUsbFixtureSky130 : public SingleEndpointUsbFixture {
public:
	std::function<void()> configure = []() {};
	void runTest() {
		configure();
		m_useSimuPhy = false;
		m_pinApplicationInterface = false;
		m_pinStatusRegister = false;
		m_maxPacketLength = 64;

		//auto device = std::make_unique<scl::Sky130Device>();
		//design.setTargetTechnology(std::move(device));

		Clock clk12{ ClockConfig{
			.absoluteFrequency = 12'000'000,
			.name = "CLK12M",
			.resetType = ClockConfig::ResetType::NONE
		} };

		ClockScope clkScp(clk12);

		setupFunction();
		{
			scl::TransactionalFifo<scl::usb::Function::StreamData> loopbackFifo{ 256 };
			m_func->attachRxFifo(loopbackFifo, 1 << 1);
			m_func->attachTxFifo(loopbackFifo, 1 << 1);
			loopbackFifo.generate();
		}

		addSimulationProcess([&]() -> SimProcess {
			co_await OnClk(clk12);
			co_await m_controller->testWindowsDeviceDiscovery();
			stopTest();
		});

		design.postprocess();

		vhdl::VHDLExport vhdl("synthesis_projects/usb_loopback_sky130/usb_loopback_sky130.vhd");
		//vhdl.targetSynthesisTool(new Yosys());
		vhdl(design.getCircuit());

		BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
	}
};

BOOST_FIXTURE_TEST_CASE(usb_loopback_sky130, WindowsDiscoveryLoopbackUsbFixtureSky130, *boost::unit_test::disabled())
{
	configure = []() { hlim::NodeGroup::configTree("scl_recoverDataDifferential*", "version", "sky130"); };
	runTest();
}

BOOST_FIXTURE_TEST_CASE(usb_to_uart_cyc1000, SingleEndpointUsbFixture, *boost::unit_test::disabled())
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

	UInt baudRate = BitWidth::last(hlim::ceil(ClockScope::getClk().absoluteFrequency()));
	UInt led = 8_b;
	led = reg(led, 0);
	pinOut(led, "LED");

	enum class SetupClassRequest { none, SET_LINE_CODING };
	Reg<Enum<SetupClassRequest>> setupClassRequest{ SetupClassRequest::none };

	m_setupCallback.push_back([&](Function& func) {
		func.addClassSetupHandler([&](const SetupPacket& setup) -> Bit {
			Bit handled = '0';
			setupClassRequest = SetupClassRequest::none;

			// SET_LINE_CODING
			IF(setup.request == 0x20 & setup.requestType == 0x21 & setup.wIndex == 0)
			{
				setupClassRequest = SetupClassRequest::SET_LINE_CODING;
				handled = '1';
			}

			// SET_LINE_CONTROL_STATE
			IF(setup.request == 0x22 & setup.requestType == 0x21 & setup.wIndex == 0)
			{
				led.lower(2_b) = setup.wValue.lower(2_b);
				handled = '1';
			}

			return handled;
		});

		func.addClassDataHandler([&](const BVec& packet) {
			IF(setupClassRequest.current() == SetupClassRequest::SET_LINE_CODING)
			{
				baudRate = (UInt)packet.lower(baudRate.width());
			}
		});

		led.msb() = func.configuration().lsb();
	});

	baudRate = reg(baudRate, 115'200);
	HCL_NAMED(baudRate);

	setupFunction();
	{
		scl::TransactionalFifo<scl::usb::Function::StreamData> host2uartFifo{ 16 };
		m_func->attachRxFifo(host2uartFifo, 1 << 1);
		Bit tx = strm::pop(host2uartFifo)
			.transform([](const scl::usb::Function::StreamData& in) { return (BVec)in.data; })
			| scl::uartTx(baudRate);
		pinOut(reg(tx, '1'), "TX");
		host2uartFifo.generate();
	}
	{
		scl::TransactionalFifo<scl::usb::Function::StreamData> uart2hostFifo{ 8 };

		Bit rx;
		scl::uartRx(reg(rx, '1'), baudRate)
			.transform([](const BVec& in) { return scl::usb::Function::StreamData{ .data = (UInt)in, .endPoint = 1 }; })
			.add(Ready{})
			| strm::push(uart2hostFifo);
		pinIn(rx, "RX");

		m_func->attachTxFifo(uart2hostFifo, 1 << 1);
		uart2hostFifo.generate();
	}

	addSimulationProcess([&]() -> SimProcess {
		co_await OnClk(clock);
		//co_await m_controller->testWindowsDeviceDiscovery();
		co_await m_controller->controlSetConfiguration(1);

		stopTest();
	});

	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/usb_to_uart_cyc1000/usb_to_uart_cyc1000.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_to_bitbang_max10deca, SingleEndpointUsbFixture, *boost::unit_test::disabled())
{
	m_useSimuPhy = false;
	m_pinApplicationInterface = false;
	m_pinStatusRegister = false;
	m_maxPacketLength = 8;

	auto device = std::make_unique<scl::IntelDevice>();
	device->setupDevice("10M50DAF672I6");
	design.setTargetTechnology(std::move(device));

	Clock clk50{ ClockConfig{
		.absoluteFrequency = 50'000'000,
		.name = "CLK50M",
		.resetType = ClockConfig::ResetType::NONE
	} };

	auto* pll2 = DesignScope::get()->createNode<scl::arch::intel::ALTPLL>();
	pll2->setClock(0, clk50);
	Clock clock = pll2->generateOutClock(0, 24, 25, 50, 0);
	ClockScope clkScp(clock);

	UInt baudRate = BitWidth::last(hlim::ceil(ClockScope::getClk().absoluteFrequency()));
	UInt led = 8_b;
	led = reg(led, 0);
	pinOut(led, "LED");

	
	setupFunction();
	BitBangEngine bitbang;
	{
		scl::TransactionalFifo<scl::usb::Function::StreamData> host2uartFifo{ 16 };
		m_func->attachRxFifo(host2uartFifo, 1 << 1);
		scl::TransactionalFifo<scl::usb::Function::StreamData> uart2hostFifo{ 16 };
		m_func->attachTxFifo(uart2hostFifo, 1 << 1);
		
		RvStream<BVec> command = strm::pop(host2uartFifo)
			.transform([](const scl::usb::Function::StreamData& in) { return (BVec)in.data; });

		bitbang.generate(move(command), 16)
			.transform([](const BVec& d) { return usb::Function::StreamData{ .data = (UInt)d, .endPoint = 1 }; })
			| strm::push(uart2hostFifo);

		bitbang.io(0).pin("SCL");
		bitbang.io(1).pin("MOSI");
		bitbang.io(2).pin("MISO");
		bitbang.io(3).pin("CS");

		pinOut(bitbang.io(0).in, "DBG_SCL");
		pinOut(bitbang.io(1).in, "DBG_MOSI");
		pinOut(bitbang.io(2).in, "DBG_MISO");
		pinOut(bitbang.io(3).in, "DBG_CS");

		for (size_t i = 0; i < 8; ++i)
			led[i] = bitbang.io(i+8).out;

		host2uartFifo.generate();
		uart2hostFifo.generate();
	}

	addSimulationProcess([&]() -> SimProcess {
		simu(bitbang.io(2).in) = '1';

		co_await OnClk(clock);
		co_await m_controller->testWindowsDeviceDiscovery();
		co_await m_controller->controlSetConfiguration(1);

		std::vector<uint8_t> commands = {
			// spi setup
			0x80, 0x00, 0x00, 0x82, 0x00, 0x00, 0x9e, 0x00, 0x00, 0x8d, 0x85, 0x86, 0x00, 0x00, 0x80, 0x0b, 0x0b, 0x86, 0x02, 0x00,
			// spi transfer, send a command byte and receive 8 bytes of data
			0xc1, 0x13, 0x07, 0xdf, 0xc1, 0x23, 0x3f, 0xc9,
		};
		co_await m_controller->transferOutBatch(1, std::as_bytes(std::span(commands)));


		std::vector<std::byte> result;
		while (result.size() < 8)
		{
			std::vector<std::byte> packet = co_await m_controller->transferInBatch(1, 64);
			for (size_t i = 0; i < packet.size(); ++i)
				BOOST_TEST((packet[i] == std::byte(0xFF)));
			result.insert(result.end(), packet.begin(), packet.end());
		}
		BOOST_TEST(result.size() == 8);

		for(size_t i = 0; i < 128; ++i)
			co_await OnClk(clock);

		stopTest();
	});

	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/usb_to_bitbang_max10deca/usb_to_bitbang_max10deca.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	BOOST_TEST(!runHitsTimeout({ 1, 1'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_resend_setup_interrupted, UsbFixture)
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
		co_await m_controller->controlSetConfiguration(1);

		co_await WaitFor({ 20, 1'000'000 });

		// receive a packet but do not ack
		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 1");
		co_await m_controller->sendToken(Pid::in, m_controller->functionAddress(), 1);
		std::vector<std::byte> data1 = co_await m_controller->bus().receive();
		BOOST_TEST(data1.size() > 3);

		// interrupt by control transfer which changes the endpoint
		sim::SimulationContext::current()->onDebugMessage(nullptr, "control");
		co_await m_controller->controlTransferOut({
			.direction = usb::EndpointDirection::out,
			.request = uint8_t(usb::SetupRequest::CLEAR_FEATURE),
			.index = 0x81
		});

		co_await WaitFor({ 20, 1'000'000 });

		sim::SimulationContext::current()->onDebugMessage(nullptr, "data 2");
		co_await m_controller->sendToken(Pid::in, m_controller->functionAddress(), 1);
		std::vector<std::byte> data2 = co_await m_controller->bus().receive();
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

BOOST_FIXTURE_TEST_CASE(usb_bit_crc5_rx_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	Bit in = pinIn().setName("in");
	Bit reset = pinIn().setName("reset");
	CombinedBitCrc crc(in, CombinedBitCrc::crc5, reset, '0');
	pinOut(crc.match(), "match");

	addSimulationProcess([&]() -> SimProcess {
		const uint16_t data[] = {
			// |<crc>|< 11b data >|
			{ 0b11101'000'00000001 },
			{ 0b11101'111'00010101 },
			{ 0b00111'101'00111010 },
			{ 0b01110'010'01110000 },
		};

		for (size_t j = 0; j < sizeof(data) / sizeof(*data); ++j)
		{
			sim::SimulationContext::current()->onDebugMessage(nullptr, "vector " + std::to_string(j));

			for (size_t i = 0; i < 16; ++i)
			{
				simu(reset) = i == 0;
				simu(in) = ((data[j] >> i) & 1) ? '1' : '0';
				co_await WaitFor({ 0, 1 });
				BOOST_TEST(simu(crc.match5()) == (i == 15));

				co_await OnClk(clock);
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_bit_crc16_rx_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	Bit in = pinIn().setName("in");
	Bit reset = pinIn().setName("reset");
	CombinedBitCrc crc(in, CombinedBitCrc::crc16, reset, '0');
	pinOut(crc.match(), "match");

	addSimulationProcess([&]() -> SimProcess {
		std::mt19937 rng{ 202201 };

		for (size_t j = 0; j < 3; ++j)
		{
			sim::SimulationContext::current()->onDebugMessage(nullptr, "vector " + std::to_string(j));

			std::vector<std::uint8_t> msg(j*2);
			for (std::uint8_t& it : msg) it = uint8_t(rng());

			size_t crc_ref = boost::crc<16, 0x8005, 0xFFFF, 0xFFFF, true, true>(msg.data(), msg.size());
			msg.push_back(uint8_t(crc_ref & 0xFF));
			msg.push_back(uint8_t(crc_ref >> 8));

			for (size_t i = 0; i < msg.size(); ++i)
			{
				for (size_t k = 0; k < 8; ++k)
				{
					simu(reset) = i == 0 && k == 0;
					simu(in) = ((msg[i] >> k) & 1) ? '1' : '0';
					co_await WaitFor({ 0, 1 });
					BOOST_TEST(simu(crc.match16()) == (i == msg.size() - 1 && k == 7));

					co_await OnClk(clock);
				}
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_bit_crc5_tx_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	Bit in = pinIn().setName("in");
	Bit reset = pinIn().setName("reset");
	Bit shiftOut = pinIn().setName("shiftOut");
	CombinedBitCrc crc(in, CombinedBitCrc::crc5, reset, shiftOut);
	pinOut(crc.out(), "out");

	addSimulationProcess([&]() -> SimProcess {
		const uint16_t data[] = {
			// |<crc>|< 11b data >|
			{ 0b11101'000'00000001 },
			{ 0b11101'111'00010101 },
			{ 0b00111'101'00111010 },
			{ 0b01110'010'01110000 },
		};

		for (size_t j = 0; j < sizeof(data) / sizeof(*data); ++j)
		{
			sim::SimulationContext::current()->onDebugMessage(nullptr, "vector " + std::to_string(j));

			simu(shiftOut) = '0';
			for (size_t i = 0; i < 11; ++i)
			{
				simu(reset) = i == 0;
				simu(in) = ((data[j] >> i) & 1) ? '1' : '0';
				co_await OnClk(clock);
			}

			simu(shiftOut) = '1';
			for (size_t i = 0; i < 5; ++i)
			{
				co_await OnClk(clock);
				size_t bit = (data[j] >> 11 >> i) & 1;
				BOOST_TEST(simu(crc.out()) == (bit == 1));
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(usb_bit_crc16_tx_test, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	Bit in = pinIn().setName("in");
	Bit reset = pinIn().setName("reset");
	Bit shiftOut = pinIn().setName("shiftOut");
	CombinedBitCrc crc(in, CombinedBitCrc::crc16, reset, shiftOut);
	pinOut(crc.out(), "out");

	addSimulationProcess([&]() -> SimProcess {
		std::mt19937 rng{ 202201 };

		for (size_t j = 0; j < 3; ++j)
		{
			sim::SimulationContext::current()->onDebugMessage(nullptr, "vector " + std::to_string(j));

			std::vector<std::uint8_t> msg(j * 2);
			for (std::uint8_t& it : msg) it = uint8_t(rng());

			simu(shiftOut) = '0';
			simu(reset) = '1';
			for (size_t i = 0; i < msg.size(); ++i)
			{
				for (size_t k = 0; k < 8; ++k)
				{
					//simu(reset) = i == 0 && k == 0;
					simu(in) = ((msg[i] >> k) & 1) ? '1' : '0';
					co_await OnClk(clock);
					simu(reset) = '0';
				}
			}

			size_t crc_ref = boost::crc<16, 0x8005, 0xFFFF, 0xFFFF, true, true>(msg.data(), msg.size());
			simu(shiftOut) = '1';
			co_await WaitFor({ 0, 1 });
			for (size_t i = 0; i < 16; ++i)
			{
				co_await OnClk(clock);
				size_t bit = (crc_ref >> i) & 1;
				BOOST_TEST(simu(crc.out()) == (bit == 1));
				simu(reset) = '0';
			}
		}
		stopTest();
	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 1, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(cyc10_pin_delay_tester, SingleEndpointUsbFixture, *boost::unit_test::disabled())
{
	//this test serves as a testbench to display the delay
	// obtained by using the pins of a cyclone 10 device on the leds of the board

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
	Clock clock = pll2->generateOutClock(0, 16, 1, 50, 0);
	ClockScope clkScp(clock);

	Bit tx;
	tx = reg(tx, '0');
	Counter delayCtr{ 8_b };
	
	IF(delayCtr.isFirst())
		tx = !tx;
	

	PinDelay generator(4ns);
	Bit rx = reg(delayChainWithTaps(tx, 7, generator), '0');

	HCL_NAMED(tx);
	HCL_NAMED(rx);
	IF(tx != rx)
		delayCtr.inc();

	UInt idleTime = 12'000'000 * 16;
	UInt simIdleTime = constructFrom(idleTime);
	simIdleTime = 12'000'000 * 16 / 1'000'000;
	idleTime.simulationOverride(simIdleTime);
	Counter idleCtr{ idleTime };

	IF(tx != rx)
		idleCtr.reset();
	ELSE{
		IF(idleCtr.isLast())
			delayCtr.reset();
	}

	pinOut(delayCtr.value(), "LED");
	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/cyc10_pin_delay_tester/cyc10_pin_delay_tester.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	runFixedLengthTest({ 100, 1'000'000 });
}


BOOST_FIXTURE_TEST_CASE(usb_hi_speed_register_delay_tester, SingleEndpointUsbFixture, *boost::unit_test::disabled())
{
	//this test is currently untested but also serves a questionable purpose since the delay
	// of a fast register chain delay is fully computable
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
	Clock clock = pll2->generateOutClock(0, 8, 1, 50, 0);
	ClockScope clkScp(clock);

	Bit tx;
	tx = reg(tx, '0');
	Counter delayCtr{ 8_b };

	IF(delayCtr.isFirst())
		tx = !tx;

	Clock fastClk = pll2->generateOutClock(1, 32, 1, 50, 0);

	Bit rx;
	{
		ClockScope fastScp(clkScp);
		Bit cdcTx = allowClockDomainCrossing(tx, clock, fastClk);
		auto regChain = [](Bit in) -> Bit {
			for (size_t i = 0; i < 30; i++)
				in = reg(in, '0');
			return in;
		};
		rx = delayChainWithTaps(tx, 1, regChain);
	}
	rx = reg(allowClockDomainCrossing(rx, fastClk, clock));

	HCL_NAMED(tx);
	HCL_NAMED(rx);
	IF(tx != rx)
		delayCtr.inc();

	Counter idleCtr{ 12'000'000 * 8 };

	IF(tx != rx)
		idleCtr.reset();
	ELSE{
		IF(idleCtr.isLast())
		delayCtr.reset();
	}

	pinOut(delayCtr.value(), "LED");
	design.postprocess();

	vhdl::VHDLExport vhdl("synthesis_projects/cyc10_reg_delay_tester/cyc10_reg_delay_tester.vhd");
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl(design.getCircuit());

	runFixedLengthTest({ 1, 1'000'000 });
}
