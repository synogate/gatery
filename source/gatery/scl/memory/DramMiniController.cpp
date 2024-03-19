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
#include "gatery/pch.h"
#include "DramMiniController.h"

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
		dramIo.cmd.dq = writeData.part(2, 0);
		dramIo.cmd.dqm = writeMask.part(2, 0);

		// the state machine orchestrates the write timing, including writeData shifting
		enum class WriteState
		{
			Idle, Wait, Preamble, WriteFirst, WriteSecond
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
			dramIo.dqsEnable = '1';
			writeState = WriteState::Preamble;
		}

		IF(writeState.current() == WriteState::Preamble)
		{
			dramIo.dqsPreamble = '0';
			writeData.part(2, 0) = writeData.part(2, 1);
			writeMask.part(2, 0) = writeMask.part(2, 1);

			writeState = WriteState::WriteFirst;
		}

		IF(writeState.current() == WriteState::WriteFirst)
		{
			writeState = WriteState::WriteSecond;
		}

		IF(writeState.current() == WriteState::WriteSecond)
		{
			dramIo.dqsEnable = '0';
			dramIo.dqsPreamble = '1';
			writeState = WriteState::Idle;
		}

		dramIo.dqsEnable = reg(dramIo.dqsEnable, '0');
		dramIo.dqsPreamble = reg(dramIo.dqsPreamble, '1');

		// read logic
		Counter readState{ 8 };
		IF(!readState.isFirst())
			readState.inc();

		IF(dramIo.cmd.cke & !dramIo.cmd.csn & dramIo.cmd.rasn & !dramIo.cmd.casn & dramIo.cmd.wen)
			readState.inc();

		dramIo.debugSignal = '0';
		BVec readData = tl.a->data.width();
		IF(readState.value() == 4)
		{
			dramIo.debugSignal = '1';
			readData.part(2, 0) = dramIo.dqIn;
		}
		IF(readState.value() == 5)
		{
			dramIo.debugSignal = '1';
			readData.part(2, 1) = dramIo.dqIn;
		}
		readData = reg(readData);
		HCL_NAMED(readData);

		(*tl.d)->data = readData;
		return tl;
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

		// we use a ddr output to phase shift by 180° for the command bus
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

		pinCmd(phy.cmd.cke, "cke");
		pinCmd(phy.cmd.csn, "csn");
		pinCmd(phy.cmd.rasn, "rasn");
		pinCmd(phy.cmd.casn, "casn");
		pinCmd(phy.cmd.wen, "wen");
		pinCmdVec(phy.cmd.ba, "ba");
		pinCmdVec(phy.cmd.a, "a");
		pinOut(phy.odt, cfg.pinPrefix + "odt");

		// CK: we do a 180° phase shift by inverting ddr inputs
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

		CC_LVDS_IOBUF dqsBuf;
		dqsBuf.voltage("1.8");
		//dqsBuf.oct(true);
		dqsBuf.delayOut(15);
		dqsBuf.pin(cfg.pinPrefix + "dqs_p", cfg.pinPrefix + "dqs_n");
		dqsBuf.disable() = !phy.dqsEnable;


		// there is a magical "10ns" dealy between DQ flank and DQS flank. While this is what we need, it is here by
		// accident and caused by LUT delays. Make sure to add 180° phase shift when switching to ODDR.
#if 0	// this is what we should do, but if we do the output buffer is always enables
		CC_ODDR dqsDDR(dqsBuf);
		dqsDDR.D0() = !phy.dqsPreamble;
		dqsDDR.D1() = '0';
#else
		dqsBuf.O() = !phy.dqsPreamble & ClockScope::getClk().clkSignal();
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
			dqBuf.disable() = !phy.dqsEnable;

			CC_ODDR dqDDR(dqBuf);
			dqDDR.clockInversion(true);
			dqDDR.D0() = phy.cmd.dq.part(2, 0)[i];
			dqDDR.D1() = phy.cmd.dq.part(2, 1)[i];

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
			dqDDR.D0() = phy.cmd.dqm.part(2, 0)[i];
			dqDDR.D1() = phy.cmd.dqm.part(2, 1)[i];

			pinOut(dqDDR.Q(), cfg.pinPrefix + "dqm" + std::to_string(i));
		}

		return phy;
	}
}
