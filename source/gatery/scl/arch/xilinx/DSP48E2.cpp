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
#include "DSP48E2.h"

#include <gatery/frontend.h>

namespace gtry::scl::arch::xilinx
{
	DSP48E2::DSP48E2() :
		ExternalModule("DSP48E2", "UNISIM", "vcomponents")
	{
		ce('1');
		rst('0');
		carryIn() = '0';
		carryInSel() = 0; // carry in
		inMode() = 0;
		opMode() = 0;
		aluMode() = 0;

		a() = 0;
		b() = 0;
		c() = 0;
		d() = 0;

		in("ACIN", 30_b) = 0;
		in("BCIN", 18_b) = 0;
		in("PCIN", 48_b) = 0;
		in("CARRYCASCIN") = '0';
		in("MULTSIGNIN") = '0';
	}

	void DSP48E2::clock(const Clock& clock)
	{
		clockIn(clock, "CLK");

		if(clock.getClk()->getRegAttribs().resetType != Clock::ResetType::NONE)
		{
			rst(clock.reset(Clock::ResetActive::HIGH));
		}
	}

	void DSP48E2::opMode(MuxW w, MuxX x, MuxY y, MuxZ z)
	{
		HCL_DESIGNCHECK(x != MuxX::m || y == MuxY::m);
		size_t mode = size_t(w) << 7 | size_t(z) << 4 | size_t(y) << 2 | size_t(x);
		opMode() = mode;
	}

	void DSP48E2::ce(const Bit& ce)
	{
		ceA1() = ce;
		ceA2() = ce;
		ceAD() = ce;
		ceAluMode() = ce;
		ceB1() = ce;
		ceB2() = ce;
		ceC() = ce;
		ceCarryIn() = ce;
		ceCtrl() = ce;
		ceD() = ce;
		ceInMode() = ce;
		ceM() = ce;
		ceP() = ce;
	}

	void DSP48E2::rst(const Bit& rst)
	{
		rstA() = rst;
		rstAllCarryIn() = rst;
		rstAluMode() = rst;
		rstB() = rst;
		rstC() = rst;
		rstCtrl() = rst;
		rstD() = rst;
		rstInMode() = rst;
		rstM() = rst;
		rstP() = rst;
	}

	SInt mulAccumulate(SInt a, SInt b, Bit restart, Bit valid, std::string_view instanceName)
	{
		Area ent{ "scl_mulAccumulate", true };
		if(!instanceName.empty())
			ent.getNodeGroup()->setInstanceName(std::string{ instanceName });

		HCL_DESIGNCHECK(a.width() <= 27_b);
		HCL_DESIGNCHECK(b.width() <= 18_b);
		HCL_NAMED(a);
		HCL_NAMED(b);
		HCL_NAMED(restart);
		HCL_NAMED(valid);

		// model
		SInt m = reg(reg(sext(a, 45_b) * sext(b, 45_b)));
		SInt acc = 48_b;
		Bit restartDelayed = reg(reg(restart));
		IF(reg(reg(valid)))
		{
			IF(restartDelayed)
				acc = 0;
			acc += sext(m);
		}
		acc = reg(acc);

		// export
		DSP48E2 dsp;
		dsp.clock(ClockScope::getClk());
		dsp.a() = (BVec)sext(a, 27_b);
		dsp.b() = (BVec)sext(b, 18_b);

		dsp.ceP() &= reg(reg(valid));

		dsp.inMode()[0] = '1'; // select A1 register
		dsp.inMode()[4] = '1'; // select B1 register

		dsp.opMode(DSP48E2::MuxW::p, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::zero);
		IF(reg(restart))
			dsp.opMode(DSP48E2::MuxW::zero, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::zero);

		acc.exportOverride((SInt)dsp.p());
		HCL_NAMED(acc);
		return acc;
	}
}
