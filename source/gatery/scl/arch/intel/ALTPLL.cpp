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
#include "ALTPLL.h"

//#include <gatery/hlim/Clock.h>

namespace gtry::scl::arch::intel 
{
	ALTPLL::ALTPLL()
	{
		// The altpll in altera_mf_components is subtly different and broken so this one needs an explicit component declaration to override that.
		m_libraryName = "";
		m_packageName = "";
		m_requiresComponentDeclaration = true;
		m_name = "ALTPLL";


		m_genericParameters["bandwidth_type"] = "AUTO";
		m_genericParameters["intended_device_family"] = "MAX 10";
		m_genericParameters["lpm_hint"] = "CBX_MODULE_PREFIX=pll_assis";
		m_genericParameters["lpm_type"] = "altpll";
		m_genericParameters["operation_mode"] = "NO_COMPENSATION";
		m_genericParameters["pll_type"] = "AUTO";
		m_genericParameters["port_activeclock"] = "PORT_UNUSED";
		m_genericParameters["port_areset"] = "PORT_UNUSED";
		m_genericParameters["port_clkbad0"] = "PORT_UNUSED";
		m_genericParameters["port_clkbad1"] = "PORT_UNUSED";
		m_genericParameters["port_clkloss"] = "PORT_UNUSED";
		m_genericParameters["port_clkswitch"] = "PORT_UNUSED";
		m_genericParameters["port_configupdate"] = "PORT_UNUSED";
		m_genericParameters["port_fbin"] = "PORT_UNUSED";
		m_genericParameters["port_inclk0"] = "PORT_UNUSED";
		m_genericParameters["port_inclk1"] = "PORT_UNUSED";
		m_genericParameters["port_locked"] = "PORT_USED";
		m_genericParameters["port_pfdena"] = "PORT_UNUSED";
		m_genericParameters["port_phasecounterselect"] = "PORT_UNUSED";
		m_genericParameters["port_phasedone"] = "PORT_UNUSED";
		m_genericParameters["port_phasestep"] = "PORT_UNUSED";
		m_genericParameters["port_phaseupdown"] = "PORT_UNUSED";
		m_genericParameters["port_pllena"] = "PORT_UNUSED";
		m_genericParameters["port_scanaclr"] = "PORT_UNUSED";
		m_genericParameters["port_scanclk"] = "PORT_UNUSED";
		m_genericParameters["port_scanclkena"] = "PORT_UNUSED";
		m_genericParameters["port_scandata"] = "PORT_UNUSED";
		m_genericParameters["port_scandataout"] = "PORT_UNUSED";
		m_genericParameters["port_scandone"] = "PORT_UNUSED";
		m_genericParameters["port_scanread"] = "PORT_UNUSED";
		m_genericParameters["port_scanwrite"] = "PORT_UNUSED";
		m_genericParameters["port_clk0"] = "PORT_UNUSED";
		m_genericParameters["port_clk1"] = "PORT_UNUSED";
		m_genericParameters["port_clk2"] = "PORT_UNUSED";
		m_genericParameters["port_clk3"] = "PORT_UNUSED";
		m_genericParameters["port_clk4"] = "PORT_UNUSED";
		m_genericParameters["port_clk5"] = "PORT_UNUSED";
		m_genericParameters["port_clkena0"] = "PORT_UNUSED";
		m_genericParameters["port_clkena1"] = "PORT_UNUSED";
		m_genericParameters["port_clkena2"] = "PORT_UNUSED";
		m_genericParameters["port_clkena3"] = "PORT_UNUSED";
		m_genericParameters["port_clkena4"] = "PORT_UNUSED";
		m_genericParameters["port_clkena5"] = "PORT_UNUSED";
		m_genericParameters["port_extclk0"] = "PORT_UNUSED";
		m_genericParameters["port_extclk1"] = "PORT_UNUSED";
		m_genericParameters["port_extclk2"] = "PORT_UNUSED";
		m_genericParameters["port_extclk3"] = "PORT_UNUSED";
		m_genericParameters["self_reset_on_loss_lock"] = "OFF";
		m_genericParameters["width_clock"] = 5;

		//m_clockNames = { "inclk" };
		//m_resetNames = { "" };
		//m_clocks.resize(1);

		resizeIOPorts(IN_COUNT, OUT_COUNT);

		declInputBitVector(IN_INCLK, "INCLK", 2);
		declInputBit(IN_FBIN, "FBIN");
		declInputBit(IN_PHASEUPDOWN, "PHASEUPDOWN");
		declInputBit(IN_PHASESTEP, "PHASESTEP");
		declInputBit(IN_PHASECOUNTERSELECT, "PHASECOUNTERSELECT");
		declInputBit(IN_PLLENA, "PLLENA");
		declInputBit(IN_CLKSWITCH, "CLKSWITCH");
		declInputBit(IN_PFDENA, "PFDENA");
		declInputBitVector(IN_C_ENA, "C_ENA", 2);
		declInputBitVector(IN_E_ENA, "E_ENA", 2);
		declInputBit(IN_CONFIGUPDATE, "CONFIGUPDATE");
		declInputBit(IN_SCANCLK, "SCANCLK");
		declInputBit(IN_SCANCLKENA, "SCANCLKENA");
		declInputBit(IN_SCANACLR, "SCANACLR");
		declInputBit(IN_SCANDATA, "SCANDATA");
		declInputBit(IN_SCANREAD, "SCANREAD");
		declInputBit(IN_SCANWRITE, "SCANWRITE");


		declOutputBitVector(OUT_CLK, "CLK", 5, "width_clock");
		declOutputBitVector(OUT_E, "E", 10);
		declOutputBitVector(OUT_CLKBAD, "CLKBAD", 10);
		declOutputBit(OUT_ACTIVECLOCK, "ACTIVECLOCK");
		declOutputBit(OUT_CLKLOSS, "CLKLOSS");
		declOutputBit(OUT_LOCKED, "LOCKED");
		declOutputBit(OUT_SCANDATAOUT, "SCANDATAOUT");
		declOutputBit(OUT_FBOUT, "FBOUT");
		declOutputBitVector(OUT_ENABLE, "ENABLE", 2);
		declOutputBitVector(OUT_SCLKOUT, "SCLKOUT", 2);
		declOutputBit(OUT_PHASEDONE, "PHASEDONE");
		declOutputBit(OUT_SCANDONE, "SCANDONE");

	}

	ALTPLL& ALTPLL::configureDeviceFamily(std::string familyName)
	{
		m_genericParameters["intended_device_family"] = familyName;
		return *this;
	}

	ALTPLL& ALTPLL::configureClock(size_t idx, size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShift)
	{
		std::string name = "clk" + std::to_string(idx);
		HCL_DESIGNCHECK(dutyCyclePercent > 0 && dutyCyclePercent < 100);

		m_genericParameters["port_" + name] = "PORT_USED";
		m_genericParameters[name + "_divide_by"] = div;
		m_genericParameters[name + "_duty_cycle"] = dutyCyclePercent;
		m_genericParameters[name + "_multiply_by"] = mul;
		m_genericParameters[name + "_phase_shift"] = phaseShift;
		return *this;
	}

	void ALTPLL::setClock(size_t idx, const Clock& clock)
	{
		HCL_ASSERT(idx == 0);
		BVec inClockVec = ConstBVec(2_b);
		inClockVec.lsb() = clock.clkSignal();
		setInput(IN_INCLK, inClockVec);
		//attachClock(clock, idx);

		const double clockPeriod = clock.absoluteFrequency().denominator() / (double)clock.absoluteFrequency().numerator();
		const size_t periodPs = (size_t)round(clockPeriod * 1e12);
		std::string name = "inclk" + std::to_string(idx);
		m_genericParameters[name + "_input_frequency"] = periodPs;
		m_genericParameters["port_" + name] = "PORT_USED";
	}

	std::unique_ptr<hlim::BaseNode> ALTPLL::cloneUnconnected() const
	{
		std::unique_ptr<BaseNode> res(new ALTPLL());
		copyBaseToClone(res.get());
		return res;
	}


}
