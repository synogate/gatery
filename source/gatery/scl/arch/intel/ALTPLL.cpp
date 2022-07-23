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
		m_libraryName = "altera_mf";
		m_packageName = "altera_mf_components";
		m_name = "ALTPLL";
		m_genericParameters["bandwidth_type"] = "\"AUTO\"";

		m_genericParameters["intended_device_family"] = "\"MAX 10\"";
		m_genericParameters["lpm_hint"] = "\"CBX_MODULE_PREFIX=pll_assis\"";
		m_genericParameters["lpm_type"] = "\"altpll\"";
		m_genericParameters["operation_mode"] = "\"NO_COMPENSATION\"";
		m_genericParameters["pll_type"] = "\"AUTO\"";
		m_genericParameters["port_activeclock"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_areset"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkbad0"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkbad1"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkloss"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkswitch"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_configupdate"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_fbin"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_inclk0"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_inclk1"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_locked"] = "\"PORT_USED\"";
		m_genericParameters["port_pfdena"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_phasecounterselect"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_phasedone"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_phasestep"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_phaseupdown"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_pllena"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scanaclr"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scanclk"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scanclkena"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scandata"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scandataout"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scandone"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scanread"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_scanwrite"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk0"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk1"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk2"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk3"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk4"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clk5"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena0"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena1"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena2"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena3"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena4"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_clkena5"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_extclk0"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_extclk1"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_extclk2"] = "\"PORT_UNUSED\"";
		m_genericParameters["port_extclk3"] = "\"PORT_UNUSED\"";
		m_genericParameters["self_reset_on_loss_lock"] = "\"OFF\"";
		m_genericParameters["width_clock"] = "5";

		//m_clockNames = { "inclk" };
		//m_resetNames = { "" };
		//m_clocks.resize(1);

		resizeInputs(IN_COUNT);
		resizeOutputs(OUT_COUNT);
		
		for (auto i : utils::Range(OUT_COUNT))
			setOutputConnectionType(i, { .interpretation = hlim::ConnectionType::BOOL, .width = 1 });
		setOutputConnectionType(OUT_CLK, hlim::ConnectionType{ .width = 5 });
	}

	ALTPLL& ALTPLL::configureDeviceFamily(std::string familyName)
	{
		m_genericParameters["intended_device_family"] = '\"' + familyName + '\"';
		return *this;
	}

	ALTPLL& ALTPLL::configureClock(size_t idx, size_t mul, size_t div, size_t dutyCyclePercent, size_t phaseShift)
	{
		std::string name = "clk" + std::to_string(idx);
		HCL_DESIGNCHECK(dutyCyclePercent > 0 && dutyCyclePercent < 100);

		m_genericParameters["port_" + name] = "\"PORT_USED\"";
		m_genericParameters[name + "_divide_by"] = std::to_string(div);
		m_genericParameters[name + "_duty_cycle"] = std::to_string(dutyCyclePercent);
		m_genericParameters[name + "_multiply_by"] = std::to_string(mul);
		m_genericParameters[name + "_phase_shift"] = '\"' + std::to_string(phaseShift) + '\"';
		return *this;
	}

	void ALTPLL::setClock(size_t idx, const Clock& clock)
	{
		HCL_ASSERT(idx == 0);
		BVec inClockVec = ConstBVec(2_b);
		inClockVec.lsb() = clock.clkSignal();
		setInput(IN_CLK, inClockVec);
		//attachClock(clock, idx);

		const double clockPeriod = clock.absoluteFrequency().denominator() / (double)clock.absoluteFrequency().numerator();
		const size_t periodPs = (size_t)round(clockPeriod * 1e12);
		std::string name = "inclk" + std::to_string(idx);
		m_genericParameters[name + "_input_frequency"] = std::to_string(periodPs);
		m_genericParameters["port_" + name] = "\"PORT_USED\"";
	}

	std::string ALTPLL::getTypeName() const
	{
		return "ALTPLL";
	}

	void ALTPLL::assertValidity() const
	{
	}

	Bit ALTPLL::getOutput(Outputs output)
	{
		return Bit(SignalReadPort(hlim::NodePort{ .node = this, .port = (size_t)output }));
	}

	BVec ALTPLL::getOutputVec(Outputs output)
	{
		return BVec(SignalReadPort(hlim::NodePort{ .node = this, .port = (size_t)output }));
	}

	std::string ALTPLL::getInputName(size_t idx) const
	{
		switch (idx) {
		case IN_CLK: return "inclk";
		case IN_FBIN: return "IN_FBIN";
		case IN_PHASEUPDOWN: return "IN_PHASEUPDOWN";
		case IN_PHASESTEP: return "IN_PHASESTEP";
		case IN_PHASECOUNTERSELECT0: return "IN_PHASECOUNTERSELECT0";
		case IN_PHASECOUNTERSELECT1: return "IN_PHASECOUNTERSELECT1";
		case IN_PHASECOUNTERSELECT2: return "IN_PHASECOUNTERSELECT2";
		case IN_PLLENA: return "IN_PLLENA";
		case IN_CLKSWITCH: return "IN_CLKSWITCH";
		case IN_PFDENA: return "IN_PFDENA";
		case IN_C0_ENA: return "IN_C0_ENA";
		case IN_E0_ENA0: return "IN_E0_ENA0";
		case IN_CONFIGUPDATE: return "IN_CONFIGUPDATE";
		case IN_SCANCLK: return "IN_SCANCLK";
		case IN_SCANCLKENA: return "IN_SCANCLKENA";
		case IN_SCANACLR: return "IN_SCANACLR";
		case IN_SCANDATA: return "IN_SCANDATA";
		case IN_SCANREAD: return "IN_SCANREAD";
		case IN_SCANWRITE: return "IN_SCANWRITE";
		default: return "";
		}
	}

	std::string ALTPLL::getOutputName(size_t idx) const
	{
		switch (idx) {
		case OUT_CLK: return "clk";
		case OUT_E0: return "OUT_E0";
		case OUT_E1: return "OUT_E1";
		case OUT_E2: return "OUT_E2";
		case OUT_E3: return "OUT_E3";
		case OUT_E4: return "OUT_E4";
		case OUT_E5: return "OUT_E5";
		case OUT_E6: return "OUT_E6";
		case OUT_E7: return "OUT_E7";
		case OUT_E8: return "OUT_E8";
		case OUT_E9: return "OUT_E9";
		case OUT_CLKBAD0: return "OUT_CLKBAD0";
		case OUT_CLKBAD1: return "OUT_CLKBAD1";
		case OUT_CLKBAD2: return "OUT_CLKBAD2";
		case OUT_CLKBAD3: return "OUT_CLKBAD3";
		case OUT_CLKBAD4: return "OUT_CLKBAD4";
		case OUT_CLKBAD5: return "OUT_CLKBAD5";
		case OUT_CLKBAD6: return "OUT_CLKBAD6";
		case OUT_CLKBAD7: return "OUT_CLKBAD7";
		case OUT_CLKBAD8: return "OUT_CLKBAD8";
		case OUT_CLKBAD9: return "OUT_CLKBAD9";
		case OUT_ACTIVECLOCK: return "OUT_ACTIVECLOCK";
		case OUT_CLKLOSS: return "OUT_CLKLOSS";
		case OUT_LOCKED: return "locked";
		case OUT_SCANDATAOUT: return "OUT_SCANDATAOUT";
		case OUT_FBOUT: return "OUT_FBOUT";
		case OUT_ENABLE0: return "OUT_ENABLE0";
		case OUT_ENABLE1: return "OUT_ENABLE1";
		case OUT_SCLKOUT0: return "OUT_SCLKOUT0";
		case OUT_SCLKOUT1: return "OUT_SCLKOUT1";
		case OUT_PHASEDONE: return "OUT_PHASEDONE";
		case OUT_SCANDONE: return "OUT_SCANDONE";

		default: return "";
		}
	}

	std::unique_ptr<hlim::BaseNode> ALTPLL::cloneUnconnected() const
	{
		ALTPLL* ptr;
		std::unique_ptr<BaseNode> res(ptr = new ALTPLL());
		copyBaseToClone(res.get());

		return res;
	}

	std::string ALTPLL::attemptInferOutputName(size_t outputPort) const
	{
		auto driver = getDriver(0);
		if (driver.node == nullptr)
			return "";
		if (driver.node->getName().empty())
			return "";

		if (outputPort == 0)
			return driver.node->getName() + "_pos";
		else
			return driver.node->getName() + "_neg";
	}
}
