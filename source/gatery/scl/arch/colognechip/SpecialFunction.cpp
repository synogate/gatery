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
#include "SpecialFunction.h"

namespace gtry::scl::arch::colognechip
{
	CC_BUFG::CC_BUFG() : ExternalModule("CC_BUFG") {
		requiresComponentDeclaration(true);
		isEntity(false);
	}

	Bit bufg(Bit I) 
	{ 
		CC_BUFG buf; 
		buf.I() = I; 
		return buf.O(); 
	}

	CC_USR_RSTN::CC_USR_RSTN() : ExternalModule("CC_USR_RSTN") {
		requiresComponentDeclaration(true);
		isEntity(false);
	}

	Bit usr_rstn() 
	{ 
		return CC_USR_RSTN{}.usr_rstn(); 
	}

	Clock globalReset(const Clock& clk)
	{
		Clock out = clk.deriveClock({ 
			.resetName = std::string(clk.name()) + "_usr_rstn",
			.resetType = ClockConfig::ResetType::ASYNCHRONOUS, 
			.resetActive = ClockConfig::ResetActive::LOW,
		});
		out.reset(usr_rstn(), ClockScope::getClk(), ClockConfig::ResetActive::LOW);
		return out;
	}

	CC_PLL::CC_PLL() : ExternalModule("CC_PLL") {
		requiresComponentDeclaration(true);
		isEntity(false);

		// we need to set filter to reduce the amount of warnings from p_r
		filter();
	}

	CC_PLL& CC_PLL::clkRef(const Clock& ref, bool user)
	{
		std::string_view portName = user ? "USR_CLK_REF" : "CLK_REF";
		clockIn(ref, portName, "USR_LOCKED_STDY_RST");
		refClkFreq(ref.absoluteFrequency());
		return *this;
	}

	Clock CC_PLL::clkOut(const ClockConfig& cfg, bool bufg)
	{
		HCL_DESIGNCHECK(cfg.absoluteFrequency);
		outClkFreq(*cfg.absoluteFrequency);

		std::string portName = "CLK0";
		if (cfg.phaseShiftDeg)
		{
			HCL_DESIGNCHECK_HINT(*cfg.phaseShiftDeg % 90 == 0, "the requested phase shift is not supported by pll");
			portName = "CLK" + std::to_string(*cfg.phaseShiftDeg % 360);
		}

		Clock clk = clockOut(portName, {}, cfg);
		if (cfg.resetType && *cfg.resetType != hlim::RegisterAttributes::ResetType::NONE)
			clk.reset(out("USR_PLL_LOCKED"), ClockScope::getClk(), ClockConfig::ResetActive::LOW);

		if (bufg)
		{
			Bit clockDummy; // has to be unassigned for simulation magic :/
			clockDummy.exportOverride(colognechip::bufg(out(portName)));
			clk.overrideClkWith(clockDummy);
		}
		return clk;
	}

	CC_PLL& CC_PLL::filter(size_t integralCoefficient, size_t proportionalCoefficient)
	{
		generic("CI_FILTER_CONST") = integralCoefficient;
		generic("CP_FILTER_CONST") = proportionalCoefficient;
		return *this;
	}

	CC_PLL& CC_PLL::outClkFreq(hlim::ClockRational freq)
	{
		double mhz = hlim::toDouble(freq / hlim::ClockRational{ 1'000'000 });
		generic("OUT_CLK") = std::to_string(mhz);
		return *this;
	}

	CC_PLL& CC_PLL::refClkFreq(hlim::ClockRational freq)
	{
		double mhz = hlim::toDouble(freq / hlim::ClockRational{ 1'000'000 });
		generic("REF_CLK") = std::to_string(mhz);
		return *this;
	}

	Clock pll(const ClockConfig& cfg)
	{
		return CC_PLL{}
			.clkRef(ClockScope::getClk())
			.clkOut(cfg);
	}
}
