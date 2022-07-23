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
#include "Phy.h"

namespace gtry::scl::usb
{
	struct UlpiIo
	{
		Clock clock = ClockConfig{
			.absoluteFrequency = 60'000'000,
		};

		Bit dataEn;
		UInt dataIn = 8_b;
		UInt dataOut = 8_b;
		Bit dir;
		Bit nxt;
		Bit stp;
		Bit cs;
		Bit reset;

		void pin(std::string_view prefix);
	};

	class UlpiSimulator : public std::enable_shared_from_this<UlpiSimulator>
	{
		enum class TokenPid
		{
			data0 = 0x3,
			data1 = 0xb,
			ack = 0x2,
			nak = 0xA,
			stall = 0xE,
			nyet = 0x6,
		};

	public:
		UlpiSimulator();

		void addSimulationProcess(UlpiIo& io);

		void writeRegister(uint8_t address, uint8_t value);
		uint8_t readRegister(uint8_t address) const;

		static uint8_t regBaseAddress(uint8_t address);
	protected:
		bool popToken(TokenPid pid);

	private:
		std::array<uint8_t, 256> m_register;

		std::queue<std::vector<uint8_t>> m_sendQueue;
		std::queue<std::vector<uint8_t>> m_recvQueue;
	};

	class Ulpi : public Phy
	{
		enum class State
		{
			// regWrite: idle > waitDirOut > setRegAddress > setRegData > stop > idle
			// regRead : idle > waitDirOut > setRegAddress > waitDirIn > getRegData > idle

			idle,
			waitDirOut,
			setRegAddress,
			setRegData,
			stop,
			waitDirIn,
			getRegData,
		};
	public:
		enum RegAddress
		{
			regVendorIdLow				= 0x00,
			regVendorIdHigh				= 0x01,
			regProductIdLow				= 0x02,
			regProductIdHigh			= 0x03,
			regFunctionControl			= 0x04,
			regInterfaceControl			= 0x07,
			regOtgControl				= 0x0A,
			regUsbInterruptEnRising		= 0x0D,
			regUsbInterruptEnFalling	= 0x10,
			regUsbInterruptStatus		= 0x13,
			regUsbInterruptLatch		= 0x14,
			regDebug					= 0x15,
			regScratch					= 0x16,
		};

		enum FunctionControl
		{
			regFuncXcvrSelect			= 0,
			regFuncTermSelect			= 2,
			regFuncOpMode				= 3,
			regFuncReset				= 5,
			regFuncSuspendM				= 6
		};

		enum OpMode
		{
			opModeHighSpeed = 0,
			opModeFullSpeed = 1,
			opModeLowSpeed = 2,
			opModeFullSpeedForLowSpeed = 3
		};

		enum OtgControl
		{
			regOtgIdPullup				= 0,
			regOtgDpPulldown			= 1,
			regOtgDmPulldown			= 2,
			regOtgDischrgVbus			= 3,
			regOtgChrgVbus				= 4,
			regOtgDrvVbus				= 5,
			regOtgDrvVbusExternal		= 6,
			regOtgUseExternalVbusIndicator = 7
		};

	public:
		Ulpi(std::string_view pinPrefix = "USB_");
		Ulpi(Ulpi&&) = default;
		Ulpi(const Ulpi&) = delete;

		UlpiIo& io() { return m_io; }

		Bit setup(Phy::OpMode mode = Phy::OpMode::FullSpeedFunction) override;
		const PhyRxStatus& status() const override { return m_status; }
		PhyTxStream& tx() override { return m_tx; }
		PhyRxStream& rx() override { return m_rx; }
		Clock& clock() override { return m_io.clock; }

		Bit regWrite(UInt address, UInt data);
		Bit regRead(UInt address, UInt& data);

	protected:
		void generate();
		void generateRegFSM();
		void generateRxStatus();
		void generateRxStream();
		void generateTxStream();

	private:
		Area m_area;
		UlpiIo m_io;

		PhyRxStatus m_status;
		PhyRxStream m_rx;
		PhyTxStream m_tx;

		Reg<Enum<State>> m_state;

		std::shared_ptr<UlpiSimulator> m_sim;

		// reg read / write command data
		UInt m_regAddr = 8_b;
		UInt m_regData = 8_b;
	};
}
