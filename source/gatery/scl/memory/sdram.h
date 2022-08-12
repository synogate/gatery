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
#include "../stream/Packet.h"
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
		uint16_t refi; // ns average refresh interval

		uint16_t wr = 2; // cycles write recovery time

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
		Nop = 0,
		Activate = 1,
		Read = 2, // use a[10] for auto precharge
		Refresh = 3, // use CKE to enter self refresh
		BurstStop = 4,
		Precharge = 5, // use a[10] to precharge all banks
		Write = 6, // use a[10] for auto precharge
		ModeRegisterSet = 7, // use bank address 1 for extended mode register
	};

	enum class DriveStrength
	{
		Weak,
		Full,
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
			UInt bank;
		};

		struct Bank
		{
			UInt bank;
		};

		using CommandStream = RvStream<Command>;
		using DataOutStream = RvPacketStream<BVec, ByteEnable>;
	public:
		Controller& timings(const Timings& timingsInNs);
		Controller& addressMap(const AddressMap& map);
		Controller& burstLimit(size_t limit);
		Controller& dataBusWidth(BitWidth width);
		Controller& pinPrefix(std::string prefix);
		Controller& driveStrength(DriveStrength value);

		virtual void generate(TileLinkUL& link);

	protected:
		virtual void initMember();
		virtual void makeBusPins(const CommandBus& bus, std::string prefix);
		virtual void makeBankState();
		virtual void makeWriteBurstAddress(CommandStream& stream);

		CommandStream makeCommandStream() const;

		virtual CommandStream initSequence() const;
		virtual CommandStream refreshSequence(const Bit& mayRefresh);

		virtual void driveCommand(CommandStream& command, DataOutStream& data);
		virtual CommandStream translateCommand(const BankState& state, const TileLinkChannelA& request) const;
		virtual DataOutStream translateCommandData(TileLinkChannelA& request, Bit& bankStall) const;

		virtual std::tuple<CommandStream, DataOutStream> bankController(TileLinkChannelA& link, BankState& state) const;

		virtual BankState updateState(const Command& cmd, const BankState& state) const;
		virtual CommandStream enforceTiming(CommandStream& command) const;

		size_t writeToReadTiming() const;
	protected:
		Area m_area = Area{ "scl_sdramController" };

		Timings m_timing;
		AddressMap m_mapping;
		size_t m_burstLimit = 1;
		BitWidth m_addrBusWidth;
		BitWidth m_dataBusWidth;
		std::string m_pinPrefix = "SDRAM_";
		DriveStrength m_driveStrength = DriveStrength::Weak;
		const bool m_useOutputRegister = true;

		Vector<BankState> m_bankState;
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
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::Controller::Command, code, address);
