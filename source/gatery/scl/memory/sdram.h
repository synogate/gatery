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
#include "../tilelink/tilelink.h"

namespace gtry::scl::sdram
{
	struct Timings
	{
		uint16_t cl;  // cycles read latency
		uint16_t rcd; // ns RAS -> CAS
		uint16_t ras; // ns RAS -> Precharge
		uint16_t rp;  // ns Precharge -> RAS
		uint16_t rc;  // ns RAS -> RAS
		uint16_t rrd; // ns RAS -> RAS (different bank)

		Timings toCycles(hlim::ClockRational memClock) const;
	};

	struct AddressMap
	{
		Selection column;
		Selection row;
		Selection bank;
	};

	struct CommandBus
	{
		Bit cke;	// Clock Enable
		Bit csn;	// Chip Select N
		Bit rasn;	// Row Access Strobe N
		Bit casn;	// Column Access Strobe N
		Bit wen;	// Write Enable N
		BVec a;		// Address
		BVec ba;	// Bank Address
		BVec dq;	// Write Data
		BVec dqm;	// Read/Write Data Mask
	};

	enum class CommandCode
	{
		// RAS = 1, CAS = 2, WE = 4
		Activate = 1,
		Read = 2, // use a[10] for auto precharge
		Refresh = 3, // use CKE to enter self refresh
		BurstStop = 4,
		Precharge = 5, // use a[10] to precharge all banks
		Write = 6, // use a[10] for auto precharge
		ModeRegisterSet = 7, // use bank address 1 for extended mode register
	};

	class Controller
	{
	public:
		struct BankState
		{
			Bit rowActive;
			BVec activeRow;
		};

		struct Command
		{
			Enum<CommandCode> code;
			BVec address;
		};

		struct Bank
		{
			BVec bank;
		};

		template<typename... T>
		using CommandStream = RvStream<BVec, ByteEnable, Command, T...>;
	public:
		Controller& timings(const Timings& timingsInNs);
		Controller& addressMap(const AddressMap& map);
		Controller& burstLimit(size_t limit);
		Controller& dataBusWidth(BitWidth width);
		Controller& pinPrefix(std::string prefix);

		virtual void generate(TileLinkUL& link);

	protected:
		virtual void makeBusPins(const CommandBus& bus, std::string prefix);

		virtual void driveCommand(CommandStream<Bank>& command);
		virtual CommandStream<> translateCommand(const BankState& state, TileLinkChannelA& request) const;
		virtual CommandStream<> enforceTiming(CommandStream<>& command) const;

		Area m_area = Area{ "scl_sdramController" };

	private:
		Timings m_timing;
		AddressMap m_mapping;
		size_t m_burstLimit = 1;
		BitWidth m_addrBusWidth;
		BitWidth m_dataBusWidth;
		std::string m_pinPrefix = "SDRAM_";
		const bool m_useOutputRegister = true;

		UInt m_rasTimer;
		CommandBus m_cmdBus;
		Bit m_dataOutEnable;
		BVec m_dataIn;
	};

	void checkModuleTiming(const CommandBus& cmd, const Timings& timing);
	BVec moduleSimulation(const CommandBus& cmd);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::CommandBus, cke, csn, rasn, casn, wen, a, ba, dq, dqm);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::Controller::BankState, rowActive, activeRow);
