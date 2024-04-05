/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include <gatery/scl_pch.h>
#include "IBUFDS_GTE4.h"

namespace gtry::scl::arch::xilinx {

	IBUFDS_GTE4::IBUFDS_GTE4(): ExternalModule("IBUFDS_GTE4", "UNISIM", "vcomponents") 
	{ 
		isEntity(false); 
		in("CEB") = '0'; 
		generic("REFCLK_HROW_CK_SEL") = "00"; 
	}

	IBUFDS_GTE4& IBUFDS_GTE4::clockInput(const Clock& inClk, Bit inClkN)
	{ 
		clockIn(inClk, "I");
		m_inClk = inClk;
		in("IB") = inClkN;
		return *this;
	}

	Clock IBUFDS_GTE4::clockOutGT()
	{
		HCL_DESIGNCHECK_HINT(m_inClk, "use clockInput first"); 
		return clockOut(*m_inClk, "O");
	}

	Clock IBUFDS_GTE4::clockOutAux()
	{
		m_auxOutputSet = true;
		HCL_DESIGNCHECK_HINT(m_inClk, "use clockInput first"); 
		if (m_auxDivideFreqBy2)
			return clockOut(*m_inClk, "ODIV2", {}, { .frequencyMultiplier = hlim::ClockRational{1,2} });
		else
			return clockOut(*m_inClk, "ODIV2");
	}

	IBUFDS_GTE4& IBUFDS_GTE4::clockEnable(Bit clockEnable) 
	{
		in("CEB") = !clockEnable;
		return *this; 
	}

	IBUFDS_GTE4& IBUFDS_GTE4::auxDivideFreqBy2() 
	{
		HCL_DESIGNCHECK_HINT(!m_auxOutputSet, "You must call this before you call clockOutAux()");
		generic("REFCLK_HROW_CK_SEL") = "01";
		m_auxDivideFreqBy2 = true; 
		return *this; 
	}
}
