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
#include <gatery/frontend/Bit.h>
#include <gatery/frontend/ExternalModule.h>
#include <gatery/scl/stream/Stream.h>

namespace gtry::scl::arch::colognechip
{
	class CC_BUFG : public ExternalModule
	{
	public:
		CC_BUFG();

		Bit& I() { return in("I"); }
		Bit O() { return out("O"); }
	};

	Bit bufg(Bit I);


	class CC_USR_RSTN : public ExternalModule
	{
	public:
		CC_USR_RSTN();

		Bit usr_rstn() { return out("USR_RSTN"); }
	};

	Bit usr_rstn();
	Clock globalReset(const Clock& clk);

	class CC_PLL : public ExternalModule
	{
	public:
		CC_PLL();

		CC_PLL& clkRef(const Clock& ref, bool user = false);
		Clock clkOut(const ClockConfig& cfg, bool bufg = true);

		Bit locked() { return out("USR_PLL_LOCKED"); }
		Bit lockedSteady() { return out("USR_PLL_LOCKED_STDY"); }
		Bit& lockedSteadyReset() { return in("USR_LOCKED_STDY_RST"); }

		CC_PLL& lockRequired(bool val) { generic("LOCK_REQ") = val ? 1 : 0; return *this; }
		CC_PLL& filter(size_t integralCoefficient = 2, size_t proportionalCoefficient = 4);
		CC_PLL& outClkFreq(hlim::ClockRational freq);
		CC_PLL& refClkFreq(hlim::ClockRational freq);
	};

	Clock pll(const Clock& src, const ClockConfig& cfg, bool userClk = false);
	Clock clkdiv(const Clock& src, ClockConfig cfg);
	Clock clkdivRegChain(const Clock& src, size_t log2div, bool buffer = true);

	class CC_CFG_CTRL : public ExternalModule
	{
	public:
		CC_CFG_CTRL();

		Bit& recfg() { return in("RECFG"); }

		Bit& en() { return in("EN"); }
		Bit& valid() { return in("VALID"); }
		BVec& data() { return in("DATA", 8_b); }
	};

	void configure(RvPacketStream<BVec>&& bitstream, CC_CFG_CTRL& ctrl);
	inline auto configure(CC_CFG_CTRL& ctrl) { return [&](RvPacketStream<BVec>&& stream) { configure(move(stream), ctrl); }; }

	Bit serdes(BVec data, const Clock& slowClk, const Clock& fastClk);
	Bit serdes(BVec data, const Clock& slowClk, const Clock& fastClk, Bit syncPulse);
}
