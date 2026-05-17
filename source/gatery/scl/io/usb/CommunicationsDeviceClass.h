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
#include "Function.h"
#include "../TxRxStream.h"

namespace gtry::scl
{
	class MemoryMap;
}

namespace gtry::scl::usb
{
	class SimuBusBase;

	// The notification endpoint is optional but Linux does not accept the device without it. It works under windows.
	void virtualCOMsetup(Function& func, uint8_t interfaceNumber, uint8_t endPoint, std::optional<uint8_t> notificationEndPoint = {}, boost::optional<Bit&> dtr = {}, boost::optional<Bit&> rts = {});

	struct StreamAdapterCfg
	{
		Phy* phy = nullptr;
		SimuBusBase** simhost = nullptr;
		std::optional<Clock> usbClk;
		std::string_view enablePinName = "USB_EN";
		size_t maxPacketSize = 8;
		size_t fifoDepth = 512;
	};

	/**
	 * @brief One stop shop for GPIO based USB Function to TxRxStream using CDC device class. Use if you need the most simple usb communication.
	 *			- It's also a great starting point for more customized USB devices.
	 *			- use usb::SimuHostController controller(*usbBus, usbFunction.descriptor()); to drive your simulation tests.
	 * @param function Function instance needs to stay alive for simulation.
	 * @param simhost Required for simulation.
	 * @param usbClk Specify a clock for usb if different from TxRxStream clock. Clock domain crossing is included using the buffer fifo's.
	 * @param enablePinName (optional) Pin name for a simple low after reset enable pin.
	 */
	Reverse<TxRxStream> streamToSerialAdapter(usb::Function& function, const StreamAdapterCfg& cfg = {});

	void uartDevice(MemoryMap& mmap, std::string mmapName, usb::Function& function, const StreamAdapterCfg& cfg = {});
}
