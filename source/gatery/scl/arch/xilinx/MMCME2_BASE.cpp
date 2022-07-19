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

#include "MMCME2_BASE.h"
#include <gatery/hlim/Clock.h>

namespace gtry::scl::arch::xilinx {

MMCME2_BASE::MMCME2_BASE()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "MMCME2_BASE";
	m_genericParameters["BANDWIDTH"] = "\"OPTIMIZED\"";

	m_genericParameters["CLKFBOUT_MULT_F"] = "2.0";
	m_genericParameters["CLKOUT0_DIVIDE_F"] = "2.0";
	m_genericParameters["CLKOUT1_DIVIDE"] = "2";
	m_genericParameters["CLKOUT2_DIVIDE"] = "2";
	m_genericParameters["CLKOUT3_DIVIDE"] = "2";
	m_genericParameters["CLKOUT4_DIVIDE"] = "2";
	m_genericParameters["CLKOUT5_DIVIDE"] = "2";
	m_genericParameters["CLKOUT6_DIVIDE"] = "2";
	m_genericParameters["DIVCLK_DIVIDE"] = "1";
	m_genericParameters["CLKFBOUT_PHASE"] = "0.0";

	m_genericParameters["CLKOUT0_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT1_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT2_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT3_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT4_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT5_DUTY_CYCLE"] = "0.5";
	m_genericParameters["CLKOUT6_DUTY_CYCLE"] = "0.5";

	m_genericParameters["CLKOUT0_PHASE"] = "0.0";
	m_genericParameters["CLKOUT1_PHASE"] = "0.0";
	m_genericParameters["CLKOUT2_PHASE"] = "0.0";
	m_genericParameters["CLKOUT3_PHASE"] = "0.0";
	m_genericParameters["CLKOUT4_PHASE"] = "0.0";
	m_genericParameters["CLKOUT5_PHASE"] = "0.0";
	m_genericParameters["CLKOUT6_PHASE"] = "0.0";


	m_genericParameters["CLKOUT4_CASCADE"] = "FALSE";
	m_genericParameters["REF_JITTER1"] = "0.01";
	m_genericParameters["STARTUP_WAIT"] = "FALSE";
	
	m_clockNames = {"CLKIN1"};
	m_resetNames = {"RST"};
	m_clocks.resize(CLK_COUNT);

	resizeInputs(IN_COUNT);
	resizeOutputs(OUT_COUNT);
	for (auto i : utils::Range(OUT_COUNT))
		setOutputConnectionType(i, {.interpretation = hlim::ConnectionType::BOOL, .width=1});


	setInput(IN_PWRDWN, '0');
}

void MMCME2_BASE::setClock(hlim::Clock *clock)
{
	attachClock(clock, 0);

	double clockPeriod = clock->absoluteFrequency().denominator() / (double) clock->absoluteFrequency().numerator();
	m_genericParameters["CLKIN1_PERIOD"] = std::to_string(clockPeriod * 1e9); // in ns
}

std::string MMCME2_BASE::getTypeName() const
{
	return "MMCME2_BASE";
}

void MMCME2_BASE::assertValidity() const
{
}

void MMCME2_BASE::setInput(Inputs input, const Bit &bit)
{
	rewireInput(input, bit.readPort());
}

Bit MMCME2_BASE::getOutput(Outputs output)
{
	return Bit(SignalReadPort(hlim::NodePort{.node = this, .port = (size_t)output}));
}


std::string MMCME2_BASE::getInputName(size_t idx) const
{
	switch (idx) {
		case IN_PWRDWN: return "PWRDWN";
		case IN_CLKFBIN: return "CLKFBIN";
		default: return "";
	}
}

std::string MMCME2_BASE::getOutputName(size_t idx) const
{
	switch (idx) {
        case OUT_CLKOUT0: return "CLKOUT0";
        case OUT_CLKOUT0B: return "CLKOUT0B";
        case OUT_CLKOUT1: return "CLKOUT1";
        case OUT_CLKOUT1B: return "CLKOUT1B";
        case OUT_CLKOUT2: return "CLKOUT2";
        case OUT_CLKOUT2B: return "CLKOUT2B";
        case OUT_CLKOUT3: return "CLKOUT3";
        case OUT_CLKOUT3B: return "CLKOUT3B";
        case OUT_CLKOUT4: return "CLKOUT4";
        case OUT_CLKOUT5: return "CLKOUT5";
        case OUT_CLKOUT6: return "CLKOUT6";
        case OUT_CLKFBOUT: return "CLKFBOUT";
        case OUT_CLKFBOUTB: return "CLKFBOUTB";
        case OUT_LOCKED: return "LOCKED";
		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> MMCME2_BASE::cloneUnconnected() const
{
	MMCME2_BASE *ptr;
	std::unique_ptr<BaseNode> res(ptr = new MMCME2_BASE());
	copyBaseToClone(res.get());

	return res;
}

std::string MMCME2_BASE::attemptInferOutputName(size_t outputPort) const
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
