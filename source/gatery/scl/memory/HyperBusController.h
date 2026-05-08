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
#include <gatery/frontend.h>
#include <gatery/scl/tilelink/tilelink.h>

namespace gtry::scl
{
	////////////////////////////// phy //////////////////////////////

	struct HyperBusPhy
	{
		// csn is instant set, delayed reset and also controls clock enable
		BVec csn;

		// rwds and dq are:
		//		send upper word at falling edge and lower word on rising edge
		//		received on rising edge, upper wird first
		//		see doc/HyperRAM planning 2.json for visuals
		Bidir<BVec> rwds = { 2_b, 2_b };
		Bidir<BVec> dq = { 16_b, 16_b };
	};

	struct HyperBusPhyConfig
	{
		std::vector<std::string> pinCsn = { "_CSn" };
		std::string_view pinClk = "_CKp";
		std::string_view pinClkN = "_CKn";	// set empty for single ended clock
		std::string_view pinRwds = "_RWDS";
		std::string_view pinDq = "_DQ";
		std::string_view pinResetN;
	};

	// NOTE: generic phy is not functional due to lack of generic ddr macros. use for testing only.
	Reverse<HyperBusPhy> genericHyperBusPhy(std::string pinPrefix = "HYPER", HyperBusPhyConfig cfg = {});

	Reverse<HyperBusPhy> gateMateHyperBusPhy(std::string pinPrefix = "HYPER", HyperBusPhyConfig cfg = {});

	////////////////////////////// controller //////////////////////////////

	struct HyperBusCommand
	{
		enum Type { write = 0, read = 1 };
		enum Space { mem = 0, reg = 1 };
		enum Burst { wrapped = 0, linear = 1 };

		Enum<Type> type;
		Enum<Space> space;
		Enum<Burst> burst;

		UInt wordAddress;
	};

	struct HyperBusController
	{
		// Note that valid, type, space and burst is directly forwarded to the output buffer and therefore require a short timing paths.
		// Proper output path delay constrants are required on these paths to catch timing problems.
		RvStream<HyperBusCommand> command;

		// Write data can be given up to three cycles after the write command.
		// This allows for write data to be buffered in FIFOs or registers.
		// For memory access it can be delayed by one more cycle.
		// Not presenting writeData in time will lead to sim_assert and undefined behaviour.
		RvPacketStream<BVec, ByteEnable> writeData = { 16_b, { Ready{}, Valid{}, Eop{}, ByteEnable{2_b} } };

		// Read data is presented on readData until one cycle after readStop is asserted. 
		// Keep readStop high to read one word only. Make sure it is low before data arives to read more words.
		// The bursting behaviour depends on the hyper bus device, burst mode and register settings.
		// Note that HyperRAM requires a transfer to not exceed tCSM (1-4 us). (4us@50MHz = 384Byte)
		Bit readStop;
		Reverse<VPacketStream<BVec>> readData = { 16_b };

		// Initial Latency can be configured using configuration register writes.
		// Writes to registers are always independent of the Initial Latency setting.
		// It is therefore possible to recude hw by setting this to a constant, independent of the device default setting.
		// A typical default is 7 and a typical value for speeds up to 85MHz is 3.
		UInt latency = 4_b;
	};

	enum class HyperBusProtocoll
	{
		HyperBus,
		OctalPeripheralInterface,
	};

	struct HyerBusControllerConfig
	{
		HyperBusProtocoll protocoll = HyperBusProtocoll::HyperBus;
	};

	/**
	 * @brief Creates a low-latency HyperBus controller. The reduced latency is achieved by not using RWDS to time read data. 
	 *			Therefore no synchronization between RWDS and controller clock domain is required.
	 *			The downside is that tCKDS + 2*propagation delay + tSETUP must be less than half clock period. 
	 *			For common HyperRAM deices this holds true until aprox. 60 MHz. Proper training or synchronization is required for faster speeds.
	 *			At Initial Latency of 3 the read latency of this controller is 7 or 10 cycles.
	 *			The controller does not issue any autonomous register accesses. Use 'space = reg' to setup the device as needed.
	 */
	Reverse<HyperBusController> hyperBusControllerLowLatency(Reverse<HyperBusPhy> phy, HyerBusControllerConfig cfg = {});

	////////////////////////////// memory adapter //////////////////////////////

	struct HyperBusTileLinkConfig
	{
		BitWidth addrW = 28_b;
		BitWidth sourceW = 0_b;
		size_t burstLimit = 64;

		UInt latency = ConstUInt(7, 3_b);
		Bit registerAccess = '0';
	};

	/**
	 * @brief Bridge between HyperBus and TileLink.
	 *			Note: The d channel needs to be ready to accept any requested response at any time. 
	 *					You need to attach a credit FIFO to the A and D channel to be TileLink compliant.
	 *					However, this adds latency and is therefore not part of this module.
	 */
	scl::TileLinkU hyperBusTileLink(Reverse<HyperBusController> controller, HyperBusTileLinkConfig cfg = {});
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::HyperBusPhy, csn, rwds, dq);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::HyperBusCommand, type, space, burst, wordAddress);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::HyperBusController, command, writeData, readStop, readData, latency);
