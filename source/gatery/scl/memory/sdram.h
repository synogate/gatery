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
#include "../ShiftReg.h"
#include "../stream/Packet.h"
#include "../tilelink/tilelink.h"

#include "SdramTimer.h"

namespace gtry::scl::sdram
{
	class Controller
	{
	public:

		struct Command
		{
			Enum<CommandCode> code;
			BVec address;
			UInt bank;
			UInt size;
			UInt source;
		};

		using CommandStream = RvStream<Command>;
		using DataOutStream = RvPacketStream<BVec, ByteEnable>;

		struct Bank
		{
			UInt bank;
		};

		struct ReadTask
		{
			Bit active;
			Bit read;
			UInt size;
			UInt source;
			UInt beats;
		};

		struct BankState
		{
			Bit rowActive;
			BVec activeRow;
		};
	public:
		Controller& timings(const Timings& timingsInNs);
		Controller& addressMap(const AddressMap& map);
		Controller& burstLimit(size_t logLimit);
		Controller& dataBusWidth(BitWidth width);
		Controller& pinPrefix(std::string prefix);
		Controller& driveStrength(DriveStrength value);
		Controller& exportClockPin(bool enable = true);

		virtual void generate(TileLinkUB& link);

	protected:
		virtual void initMember();
		virtual void makeBusPins(const CommandBus& bus, std::string prefix);
		virtual void makeBankState();
		virtual void makeWriteBurstAddress(CommandStream& stream);
		virtual void makeReadQueue(const CommandStream& cmd);
		virtual void setMaskForRead(DataOutStream& data);
		virtual void setResponse(TileLinkChannelD& response);

		CommandStream makeCommandStream() const;

		virtual CommandStream initSequence() const;
		virtual CommandStream refreshSequence(const Bit& mayRefresh);

		virtual void driveCommand(CommandStream& command, DataOutStream& data);
		virtual CommandStream translateCommand(const BankState& state, const TileLinkChannelA& request) const;
		virtual DataOutStream translateCommandData(const TileLinkChannelA& request) const;

		virtual std::tuple<CommandStream, DataOutStream> bankController(TileLinkChannelA& link, BankState& state, UInt bank) const;

		virtual BankState updateState(const Command& cmd, const BankState& state) const;
		virtual CommandStream enforceTiming(CommandStream& command, UInt bank) const;

		size_t writeToReadTiming() const;
		size_t readDelay() const;
	protected:
		Area m_area = Area{ "scl_sdramController" };

		Timings m_timing;
		AddressMap m_mapping;
		size_t m_burstLimit = 1;
		BitWidth m_addrBusWidth;
		BitWidth m_dataBusWidth;
		BitWidth m_sourceW;
		std::string m_pinPrefix = "SDRAM_";
		DriveStrength m_driveStrength = DriveStrength::Weak;
		const bool m_useOutputRegister = true;
		const bool m_useInputRegister = true;
		bool m_exportClockPin = true;

		Vector<BankState> m_bankState;
		CommandBus m_cmdBus;
		Bit m_dataOutEnable;
		BVec m_dataIn;

		ShiftReg<ReadTask> m_readQueue;
		std::shared_ptr<SdramTimer> m_timer;
	};

	enum class Standard
	{
		sdram,
		ddr2,
	};

	void checkModuleTiming(const CommandBus& cmd, const Timings& timing);
	VStream<BVec> moduleSimulation(const CommandBus& cmd, Standard standard = Standard::sdram);
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::Controller::BankState, rowActive, activeRow);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::Controller::Command, code, address, size, source);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::Controller::ReadTask, active, read, size, source, beats);
