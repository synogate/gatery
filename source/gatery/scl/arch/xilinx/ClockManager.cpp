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
#include "ClockManager.h"

namespace gtry::scl::arch::xilinx
{
	ClockManager::ClockManager(std::string_view macroName) :
		ExternalModule(macroName, "UNISIM", "vcomponents")
	{
		in("PWRDWN") = '0';
		in("CLKFBIN") = out("CLKFBOUT");
	}

	Bit ClockManager::locked()
	{
		return out("LOCKED");
	}

	void ClockManager::clkIn(size_t index, const Clock& clk)
	{
		m_clkIn.emplace(index, clk);

		std::string portName = clkInPrefix(index);
		clockIn(clk, portName);

		double periodNs = (clk.absoluteFrequency().denominator() * 1'000'000'000.0) / clk.absoluteFrequency().numerator();
		generic(portName + "_PERIOD") = periodNs;

		ClockScope clkScope{ clk };
		in("RST") = clk.reset(Clock::ResetActive::HIGH);
		locked(); // create pin in input clock domain
	}

	void ClockManager::vcoCfg(size_t div, size_t mul)
	{
		m_vcoDiv = div;
		m_vcoMul = mul;

		generic("DIVCLK_DIVIDE") = div;
		generic(m_multiplierName) = double(mul);
	}

	Clock ClockManager::clkOut(std::string_view name, size_t index, size_t counterDiv, bool inverted)
	{
		HCL_DESIGNCHECK_HINT(m_clkIn.contains(0), "clkIn 0 needs to be connected to derive clocks from");
		HCL_DESIGNCHECK_HINT(m_vcoDiv && m_vcoMul, "VCO not configured");

		std::string portName = "CLKOUT" + std::to_string(index);
		if(index == 0)
			generic(portName + "_DIVIDE_F") = double(counterDiv);
		else
			generic(portName + "_DIVIDE") = counterDiv;

		if(inverted)
			portName += "B";

		Clock& in0 = m_clkIn.find(0)->second;
		Clock out = clockOut(
			in0, portName, {},
			ClockConfig{ 
				.frequencyMultiplier = hlim::ClockRational{ m_vcoMul, m_vcoDiv * counterDiv },
				.name = std::string{name},
				//.phaseSynchronousWithParent = !inverted,
				.resetType = Clock::ResetType::ASYNCHRONOUS,
				.resetActive = Clock::ResetActive::LOW
			}
		);
		out.reset(locked());
		return out;
	}

	std::string ClockManager::clkInPrefix(size_t index) const
	{
		return "CLKIN" + std::to_string(index + 1);
	}
}
