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
#include "DramMiniController.h"
#include "sdram.h"

#include <gatery/scl/arch/colognechip/io.h>
#include <gatery/scl/arch/colognechip/SpecialFunction.h>

namespace gtry::scl::sdram
{
	scl::TileLinkUL miniController(PhyInterface& dramIo, MiniControllerConfig cfg)
	{
		Area ent{ "scl_sdramMiniController", true };

		TileLinkUL tl = tileLinkInit<TileLinkUL>(3_b, dramIo.cmd.dq.width() * 2, cfg.sourceW);
		**tl.d = reg(tileLinkDefaultResponse(*tl.a));
		valid(*tl.d) = reg(valid(tl.a), '0');
		ready(tl.a) = ready(*tl.d);

		// address 0 is directly forwarded to the dram command bus
		// cke is held stable
		IF(valid(tl.a) & tl.a->isPut() & tl.a->address == 0)
		{
			dramIo.cmd.cke = tl.a->data[0];
			dramIo.odt = tl.a->data[1];
		}
		dramIo.cmd.cke = reg(dramIo.cmd.cke, '0');
		dramIo.odt = reg(dramIo.odt, '0');
		dramIo.cmd.csn = tl.a->data[2] | !(transfer(tl.a) & tl.a->isPut() & tl.a->address == 0);
		dramIo.cmd.rasn = tl.a->data[3];
		dramIo.cmd.casn = tl.a->data[4];
		dramIo.cmd.wen = tl.a->data[5];
		dramIo.cmd.ba = tl.a->data(6, dramIo.cmd.ba.width());
		dramIo.cmd.a = tl.a->data(6 + dramIo.cmd.ba.width().bits(), dramIo.cmd.a.width());

		// address 1 use used for writing data and reading data from dq
		// write
		
		BVec writeData = tl.a->data.width();
		BVec writeMask = tl.a->mask.width();
		IF(valid(tl.a) & tl.a->isPut() & tl.a->address == 4)
		{
			writeData = tl.a->data;
			writeMask = ~tl.a->mask;
		}
		writeData = reg(writeData);	HCL_NAMED(writeData);
		writeMask = reg(writeMask); HCL_NAMED(writeMask);

		// the state machine orchestrates the write timing, including writeData shifting
		enum class WriteState
		{
			Idle, Wait, WriteFirst, WriteSecond
		};
		Reg<Enum<WriteState>> writeState{ WriteState::Idle };
		writeState.setName("writeState");

		IF(writeState.current() == WriteState::Idle)
		{
			IF(dramIo.cmd.cke & !dramIo.cmd.csn & dramIo.cmd.rasn & !dramIo.cmd.casn & !dramIo.cmd.wen)
				writeState = WriteState::Wait;
		}

		IF (writeState.current() == WriteState::Wait)
		{
			writeState = WriteState::WriteFirst;
		}

		dramIo.dqWriteValid = '0';
		dramIo.cmd.dq = writeData.part(2, 0);
		dramIo.cmd.dqm = writeMask.part(2, 0);
		IF(writeState.current() == WriteState::WriteFirst)
		{
			dramIo.dqWriteValid = '1';
			writeState = WriteState::WriteSecond;
		}

		IF(writeState.current() == WriteState::WriteSecond)
		{
			dramIo.dqWriteValid = '1';
			dramIo.cmd.dq = writeData.part(2, 1);
			dramIo.cmd.dqm = writeMask.part(2, 1);
			writeState = WriteState::Idle;
		}

		// read logic
		BVec readData = tl.a->data.width();
		IF(dramIo.dqReadValid)
		{
			readData.part(2, 0) = readData.part(2, 1);
			readData.part(2, 1) = dramIo.dqIn;
		}
		readData = reg(readData);
		HCL_NAMED(readData);

		(*tl.d)->data = readData;
		return tl;
	}

	scl::TileLinkUL miniControllerMappedMemory(PhyInterface& dramIo, BitWidth sourceW)
	{
		Area ent{ "scl_miniControllerMappedMemory", true };

		const hlim::ClockRational tREFI{ 7'800, 1'000'000'000 }; // 7.8 us
		const hlim::ClockRational tRFC{ 128, 1'000'000'000 }; // 127.5 ns

		scl::TileLinkUL tl = tileLinkInit<TileLinkUL>(
			dramIo.cmd.a.width() + dramIo.cmd.ba.width() + 10_b,
			dramIo.cmd.dq.width() * 2,
			sourceW
		);
		valid(*tl.d) = '0';

		scl::TileLinkChannelA a = regDownstream(move(tl.a));
		HCL_NAMED(a);
		ready(a) = '0';
		**tl.d = tileLinkDefaultResponse(*a);

		BitWidth addrWordW{ utils::Log2C(a->data.width().bytes()) };

		enum class State
		{
			Idle, Cas, Wait, First, Second, Ack, Recovery
		};

		Reg<Enum<State>> state{ State::Idle };
		state.setName("state");

		scl::Counter refreshInterval{ hlim::floor(tREFI * ClockScope::getClk().absoluteFrequency()) };
		Bit refreshEnabled = scl::flag(valid(a), '0');
		HCL_NAMED(refreshEnabled);
		IF(refreshEnabled)
			refreshInterval.inc();
		Bit refreshReq;
		IF(refreshInterval.isLast())
			refreshReq = '1';
		refreshReq = reg(refreshReq, '0');
		HCL_NAMED(refreshReq);

		IF(state.current() == State::Idle)
		{
			IF(valid(a))
			{
				// activate command
				dramIo.cmd.csn = '0';
				dramIo.cmd.rasn = '0';
				dramIo.cmd.casn = '1';
				dramIo.cmd.wen = '1';
				dramIo.cmd.a = (BVec)a->address.upper(dramIo.cmd.a.width());
				dramIo.cmd.ba = (BVec)a->address(10, dramIo.cmd.ba.width());
				state = State::Cas;
			}

			IF(refreshReq)
			{
				// activate command
				dramIo.cmd.csn = '0';
				dramIo.cmd.rasn = '0';
				dramIo.cmd.casn = '0';
				dramIo.cmd.wen = '1';

				refreshReq = '0';
				state = State::Recovery;
			}
		}

		IF(state.current() == State::Cas)
		{
			// cas command
			dramIo.cmd.csn = '0';
			dramIo.cmd.rasn = '1';
			dramIo.cmd.casn = '0';
			dramIo.cmd.wen = a->isGet();
			dramIo.cmd.a = (BVec)zext(cat('1', a->address(addrWordW.bits(), 10_b - addrWordW), ConstUInt(0, addrWordW)));
			dramIo.cmd.ba = (BVec)a->address(10, dramIo.cmd.ba.width());
			state = State::Wait;
		}

		IF(state.current() == State::Wait)
		{
			state = State::First;
		}

		IF(state.current() == State::First)
		{
			dramIo.dqWriteValid = a->isPut();
			dramIo.cmd.dq = a->data.part(2, 0);
			dramIo.cmd.dqm = ~a->mask.part(2, 0);

			IF(a->isPut() | dramIo.dqReadValid)
				state = State::Second;
		}

		IF(state.current() == State::Second)
		{
			dramIo.dqWriteValid = a->isPut();
			dramIo.cmd.dq = a->data.part(2, 1);
			dramIo.cmd.dqm = ~a->mask.part(2, 1);
			state = State::Ack;
		}

		IF(state.current() == State::Ack)
		{
			valid(*tl.d) = '1';
			IF(transfer(*tl.d))
			{
				ready(a) = '1';
				state = State::Recovery;
			}
		}

		size_t recoveryCyclesRefresh = hlim::ceil(tRFC * ClockScope::getClk().absoluteFrequency());
		Counter recoveryCounter{ std::max<size_t>(recoveryCyclesRefresh, 4) };
		IF(state.current() == State::Recovery)
		{
			recoveryCounter.inc();
			IF(recoveryCounter.isLast())
				state = State::Idle;
		}

		BVec readData = tl.a->data.width();
		IF(dramIo.dqReadValid)
		{
			readData.part(2, 0) = readData.part(2, 1);
			readData.part(2, 1) = dramIo.dqIn;
		}
		readData = reg(readData);
		HCL_NAMED(readData);
		(*tl.d)->data = readData;

		HCL_NAMED(tl);
		return tl;
	}

	void miniControllerSimulation(PhyInterface& dramIo)
	{
		scl::VStream<BVec> outData = moduleSimulation(dramIo.cmd, Standard::ddr2);
		pinOut(*outData, "DRAM_SIMU_DQ");

		dramIo.dqReadValid.simulationOverride(valid(outData));
		dramIo.dqIn.simulationOverride(*outData);
	}

	PhyInterface phyGateMateDDR2(PhyGateMateDDR2Config cfg)
	{
		using namespace arch::colognechip;
		Area ent{ "scl_phyGateMateDDR2", true };

		PhyInterface phy{
			.cmd = {
				.a = cfg.addrW,
				.ba = 3_b,
				.dq = cfg.dqW * 2,
				.dqm = cfg.dqW * 2 / 8,
			},
			.dqIn = cfg.dqW * 2,
		};
		HCL_NAMED(phy);

		// we use a ddr output to phase shift by 180� for the command bus
		auto pinCmd = [&](const Bit& pin, const char* name) {
			CC_ODDR ddr;
			ddr.D0() = pin;
			ddr.D1() = pin;
			pinOut(ddr.Q(), cfg.pinPrefix + name);
		};

		auto pinCmdVec = [&](const BVec& pin, const char* name) {
			BVec out = ConstBVec(pin.width());
			for (int i = 0; i < pin.size(); i++)
			{
				CC_ODDR ddr;
				ddr.D0() = pin[i];
				ddr.D1() = pin[i];
				out[i] = ddr.Q();
			}
			pinOut(out, cfg.pinPrefix + name);
		};

		CommandBus outCmd = reg(phy.cmd);
		pinCmd(outCmd.cke, "cke");
		pinCmd(outCmd.csn, "csn");
		pinCmd(outCmd.rasn, "rasn");
		pinCmd(outCmd.casn, "casn");
		pinCmd(outCmd.wen, "wen");
		pinCmdVec(outCmd.ba, "ba");
		pinCmdVec(outCmd.a, "a");
		pinOut(phy.odt, cfg.pinPrefix + "odt");

		// CK: we do a 180� phase shift by inverting ddr inputs
		// fake differential signal as long as not located on ball pairs
		CC_OBUF clkBufP, clkBufN;
		clkBufP.voltage("1.8");
		clkBufN.voltage("1.8");
		pinOut(clkBufP.pad(), cfg.pinPrefix + "ck_p");
		pinOut(clkBufN.pad(), cfg.pinPrefix + "ck_n");
		CC_ODDR clkDDRp(clkBufP);
		clkDDRp.D0() = '1';
		clkDDRp.D1() = '0';
		CC_ODDR clkDDRn(clkBufN);
		clkDDRn.D0() = '0';
		clkDDRn.D1() = '1';

		Bit dqsEnable = reg(phy.dqWriteValid | reg(phy.dqWriteValid, '0'), '0');
		HCL_NAMED(dqsEnable);
		Bit preamble;
		preamble = !dqsEnable;
		preamble = reg(preamble, '1');
		HCL_NAMED(preamble);

		CC_LVDS_IOBUF dqsBuf;
		dqsBuf.voltage("1.8");
		//dqsBuf.oct(true);
		dqsBuf.delayOut(15);
		dqsBuf.pin(cfg.pinPrefix + "dqs_p", cfg.pinPrefix + "dqs_n");
		dqsBuf.disable() = !dqsEnable;


		// there is a magical "10ns" dealy between DQ flank and DQS flank. While this is what we need, it is here by
		// accident and caused by LUT delays. Make sure to add 180� phase shift when switching to ODDR.
#if 0	// this is what we should do, but if we do the output buffer is always enables
		CC_ODDR dqsDDR(dqsBuf);
		dqsDDR.D0() = !phy.dqsPreamble;
		dqsDDR.D1() = '0';
#else
		dqsBuf.O() = !preamble & ClockScope::getClk().clkSignal();
#endif
		Clock dqsClk{ ClockConfig{
			.absoluteFrequency = ClockScope::getClk().absoluteFrequency(), 
			.name = "dqsClk", 
			.resetType = ClockConfig::ResetType::NONE
		} };
		Bit dqsClkSignal;
		dqsClkSignal.exportOverride(dqsBuf.I());
		dqsClk.overrideClkWith(dqsClkSignal);

		// DQ
		phy.dqIn = ConstBVec(cfg.dqW * 2);
		for (size_t i = 0; i < phy.cmd.dq.size() / 2; i++)
		{
			CC_IOBUF dqBuf;
			dqBuf.voltage("1.8");
			dqBuf.pin(cfg.pinPrefix + "dq" + std::to_string(i));
			dqBuf.disable() = !dqsEnable;

			CC_ODDR dqDDR(dqBuf);
			dqDDR.clockInversion(true);
			dqDDR.D0() = reg(phy.cmd.dq.part(2, 0)[i]);
			dqDDR.D1() = reg(phy.cmd.dq.part(2, 1)[i]);

			CC_IDDR dqInDDR(dqBuf);
			dqInDDR.clk(dqsClk);
			phy.dqIn.part(2, 0)[i] = dqInDDR.Q0();
			phy.dqIn.part(2, 1)[i] = dqInDDR.Q1();
		}

		// DQM
		for (size_t i = 0; i < phy.cmd.dqm.size() / 2; ++i)
		{
			CC_ODDR dqDDR;
			dqDDR.clockInversion(true);
			dqDDR.D0() = reg(phy.cmd.dqm.part(2, 0)[i]);
			dqDDR.D1() = reg(phy.cmd.dqm.part(2, 1)[i]);

			pinOut(dqDDR.Q(), cfg.pinPrefix + "dqm" + std::to_string(i));
		}

		
		{	// read timing logic
			Counter readState{ 8 };
			IF(!readState.isFirst())
				readState.inc();

			IF(phy.cmd.cke & !phy.cmd.csn & phy.cmd.rasn & !phy.cmd.casn & phy.cmd.wen)
				readState.inc();

			size_t readDelay = 5;
			phy.dqReadValid = readState.value() == readDelay | readState.value() == readDelay + 1;
		}
		return phy;
	}
}
