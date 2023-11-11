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

#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::xilinx
{
	class DSP48E2 : public ExternalModule
	{
	public:
		enum class MuxW { zero, p, rnd, c };
		enum class MuxX { zero, m, p, ab };
		enum class MuxY { zero, m, fullmask, c };
		enum class MuxZ { zero, pcin, p, c, p_extend, pcin17, p17 };

	public:
		DSP48E2();

		void clock(const Clock& clock);

		BVec& a() { return in("A", 30_b); }
		BVec& b() { return in("B", 18_b); }
		BVec& c() { return in("C", 48_b); }
		BVec& d() { return in("D", 27_b); }

		Bit& carryIn() { return in("CARRYIN"); }
		BVec& carryInSel() { return in("CARRYINSEL", 3_b); }

		BVec& inMode() { return in("INMODE", 5_b); }
		BVec& opMode() { return in("OPMODE", 9_b); }
		BVec& aluMode() { return in("ALUMODE", 4_b); }
		void opMode(MuxW w, MuxX x, MuxY y, MuxZ z);

		void ce(const Bit& ce); // connect all clock enables
		Bit& ceA1() { return in("CEA1"); }
		Bit& ceA2() { return in("CEA2"); }
		Bit& ceAD() { return in("CEAD"); }
		Bit& ceAluMode() { return in("CEALUMODE"); }
		Bit& ceB1() { return in("CEB1"); }
		Bit& ceB2() { return in("CEB2"); }
		Bit& ceC() { return in("CEC"); }
		Bit& ceCarryIn() { return in("CECARRYIN"); }
		Bit& ceCtrl() { return in("CECTRL"); }
		Bit& ceD() { return in("CED"); }
		Bit& ceInMode() { return in("CEINMODE"); }
		Bit& ceM() { return in("CEM"); }
		Bit& ceP() { return in("CEP"); }

		void rst(const Bit& rst); // connect all resets
		Bit& rstA() { return in("RSTA"); }
		Bit& rstAllCarryIn() { return in("RSTALLCARRYIN"); }
		Bit& rstAluMode() { return in("RSTALUMODE"); }
		Bit& rstB() { return in("RSTB"); }
		Bit& rstC() { return in("RSTC"); }
		Bit& rstCtrl() { return in("RSTCTRL"); }
		Bit& rstD() { return in("RSTD"); }
		Bit& rstInMode() { return in("RSTINMODE"); }
		Bit& rstM() { return in("RSTM"); }
		Bit& rstP() { return in("RSTP"); }

		// outputs
		BVec p() { return out("P", 48_b); }
		BVec xorOut() { return out("XOROUT", 8_b); }
		BVec carryOut() { return out("CARRYOUT", 4_b); }

		// needs specific pattern detector config to work
		Bit overflow() { return out("OVERFLOW"); } 
		Bit underflow() { return out("UNDERFLOW"); }
		Bit patternDetect() { return out("PATTERNDETECT"); }
		Bit patternDetectB() { return out("PATTERNBDETECT"); }
	};

	// DSP48E2 wrapper for macc including simulation model
	SInt mulAccumulate(SInt a, SInt b, Bit restart, std::string_view instanceName = {});
}
