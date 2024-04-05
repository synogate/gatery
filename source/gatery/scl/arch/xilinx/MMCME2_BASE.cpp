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

#include "MMCME2_BASE.h"
#include <gatery/hlim/Clock.h>

namespace gtry::scl::arch::xilinx {

MMCME2_BASE::MMCME2_BASE()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "MMCME2_BASE";
	
	m_genericParameters["BANDWIDTH"] = "OPTIMIZED";

	m_genericParameters["CLKFBOUT_MULT_F"] = 2.0;
	m_genericParameters["CLKOUT0_DIVIDE_F"] = 2.0;
	m_genericParameters["CLKOUT1_DIVIDE"] = 2;
	m_genericParameters["CLKOUT2_DIVIDE"] = 2;
	m_genericParameters["CLKOUT3_DIVIDE"] = 2;
	m_genericParameters["CLKOUT4_DIVIDE"] = 2;
	m_genericParameters["CLKOUT5_DIVIDE"] = 2;
	m_genericParameters["CLKOUT6_DIVIDE"] = 2;
	m_genericParameters["DIVCLK_DIVIDE"] = 1;
	m_genericParameters["CLKFBOUT_PHASE"] = 0.0;

	m_genericParameters["CLKOUT0_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT1_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT2_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT3_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT4_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT5_DUTY_CYCLE"] = 0.5;
	m_genericParameters["CLKOUT6_DUTY_CYCLE"] = 0.5;

	m_genericParameters["CLKOUT0_PHASE"] = 0.0;
	m_genericParameters["CLKOUT1_PHASE"] = 0.0;
	m_genericParameters["CLKOUT2_PHASE"] = 0.0;
	m_genericParameters["CLKOUT3_PHASE"] = 0.0;
	m_genericParameters["CLKOUT4_PHASE"] = 0.0;
	m_genericParameters["CLKOUT5_PHASE"] = 0.0;
	m_genericParameters["CLKOUT6_PHASE"] = 0.0;


	m_genericParameters["CLKOUT4_CASCADE"] = false;
	m_genericParameters["REF_JITTER1"] = 0.01;
	m_genericParameters["STARTUP_WAIT"] = false;
	
	m_clockNames = {"CLKIN1"};
	m_resetNames = {"RST"};
	m_clocks.resize(CLK_COUNT);

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_PWRDWN, "PWRDWN", BitFlavor::STD_ULOGIC);
	declInputBit(IN_CLKFBIN, "CLKFBIN", BitFlavor::STD_ULOGIC);


    declOutputBit(OUT_CLKOUT0, "CLKOUT0", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT0B, "CLKOUT0B", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT1, "CLKOUT1", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT1B, "CLKOUT1B", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT2, "CLKOUT2", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT2B, "CLKOUT2B", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT3, "CLKOUT3", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT3B, "CLKOUT3B", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT4, "CLKOUT4", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT5, "CLKOUT5", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKOUT6, "CLKOUT6", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKFBOUT, "CLKFBOUT", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_CLKFBOUTB, "CLKFBOUTB", BitFlavor::STD_ULOGIC);
    declOutputBit(OUT_LOCKED, "LOCKED", BitFlavor::STD_ULOGIC);

	setInput(IN_PWRDWN, '0');
}

void MMCME2_BASE::setClock(hlim::Clock *clock)
{
	attachClock(clock, 0);

	double clockPeriod = clock->absoluteFrequency().denominator() / (double) clock->absoluteFrequency().numerator();
	m_genericParameters["CLKIN1_PERIOD"] = clockPeriod * 1e9; // in ns
}


std::unique_ptr<hlim::BaseNode> MMCME2_BASE::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new MMCME2_BASE());
	copyBaseToClone(res.get());

	return res;
}

}
