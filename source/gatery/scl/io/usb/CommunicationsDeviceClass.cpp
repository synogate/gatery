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
#include "CommunicationsDeviceClass.h"
#include "GpioPhy.h"
#include "../../stream/strm.h"

namespace gtry::scl::usb
{
	void virtualCOMsetup(Function& func, uint8_t interfaceNumber, uint8_t endPoint, std::optional<uint8_t> notificationEndPoint, boost::optional<Bit&> dtr, boost::optional<Bit&> rts)
	{
		// CDC Interface
		func.descriptor().add(InterfaceDescriptor{
			.Class = ClassCode::Communications_and_CDC_Control,
			.SubClass = 2,
			.Protocol = 1,
			});

			// CDC - Header Functional Descriptor (CDC 1.10)
		func.descriptor().add(std::vector<uint8_t>{ 0x05, 0x24, 0x00, 0x10, 0x01 });
		// CDC - Call Management Functional Descriptor (capabilities = 0, data interface for control = 1)
		func.descriptor().add(std::vector<uint8_t>{ 0x05, 0x24, 0x01, 0x00, 0x00 });
		// CDC - Abstract Control Management Functional Descriptor (capabilities = 0)
		func.descriptor().add(std::vector<uint8_t>{ 0x04, 0x24, 0x02, 0x00 });
		// CDC - Union Functional Descriptor (Interface 0 and 1 are a union)
		func.descriptor().add(std::vector<uint8_t>{ 0x05, 0x24, 0x06, interfaceNumber, uint8_t(interfaceNumber + 1) });

		if (notificationEndPoint)
		{
			// optional notification endpoint
			func.descriptor().add(EndpointDescriptor{
				.Address = EndpointAddress::create(*notificationEndPoint, EndpointDirection::in),
				.Attributes = 3, // Interrupt
				});
		}

		// Data Interface
		func.descriptor().add(InterfaceDescriptor{
			.Class = ClassCode::CDC_Data,
			});
		func.descriptor().add(EndpointDescriptor{
			.Address = EndpointAddress::create(endPoint, EndpointDirection::in),
			});
		func.descriptor().add(EndpointDescriptor{
			.Address = EndpointAddress::create(endPoint, EndpointDirection::out),
			});

		func.addClassSetupHandler([=](const SetupPacket& setup) {

			Bit handled = '0';
			IF(setup.request == 0x20)
			{
				// accept SET_LINE_CODING commands and ignore setting
				handled = '1';
			}

			IF(setup.request == 0x22 & setup.requestType == 0x21 & setup.wIndex == interfaceNumber)
			{
				// accept SET_LINE_CONTROL_STATE
				if (dtr)
					*dtr = setup.wValue[0];
				if (rts)
					*rts = setup.wValue[1];
				handled = '1';
			}

			return handled;
			});
	}

	Reverse<TxRxStream> streamToSerialAdapter(usb::Function& function, usb::SimuBusBase** simhost, std::string_view enablePinName)
	{
		if(!enablePinName.empty())
		{
			Counter usbEnDelay{ 500ms };
			IF(!usbEnDelay.isLast())
				usbEnDelay.inc();
			pinOut(usbEnDelay.isLast(), std::string{ enablePinName });
		}

		// setup cdc descriptor for single serial interface
		usb::Descriptor& desc = function.descriptor();
		desc.add(scl::usb::DeviceDescriptor{ .Class = scl::usb::ClassCode::Communications_and_CDC_Control });
		desc.add(scl::usb::ConfigurationDescriptor{});
		scl::usb::virtualCOMsetup(function, 0, 1, 2);

		desc.changeMaxPacketSize(8);
		desc.finalize();

		// create phy and return host for simulation processes
		usb::SimuBusBase* host = &function.setup<usb::GpioPhy>();
		if (simhost)
			*simhost = host;

		// RX
		scl::TransactionalFifo<scl::usb::Function::StreamData> rxFifo{ 128 };
		function.attachRxFifo(rxFifo, 1 << 1);
		Reverse<TxRxStream> ret(TxRxStream{
			.tx = { 8_b },
			.rx = strm::pop(rxFifo) | strm::transform([](const auto& sd) { return (BVec)sd.data; }),
			});
		rxFifo.generate();

		// TX
		scl::TransactionalFifo<scl::usb::Function::StreamData> txFifo{ 128 };
		function.attachTxFifo(txFifo, 1 << 1);

		move(ret->tx)
			| strm::transform([](const BVec& d) { return scl::usb::Function::StreamData{ .data = (UInt)d, .endPoint = 1, }; })
			| strm::push(txFifo);
		txFifo.generate();

		return ret;
	}
}
