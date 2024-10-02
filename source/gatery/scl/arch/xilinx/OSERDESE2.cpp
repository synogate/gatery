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
#include "OSERDESE2.h"

namespace gtry::scl::arch::xilinx {

OSERDESE2::OSERDESE2(size_t width)
{
	m_libraryName = "UNIMACRO";
	m_packageName = "VCOMPONENTS";
	m_name = "OSERDESE2";
	m_isEntity = false;
	m_genericParameters["DATA_RATE_OQ"] = "DDR";	// DDR, SDR
	m_genericParameters["DATA_RATE_TQ"] = "DDR";	// DDR, BUF, SDR
	HCL_DESIGNCHECK_HINT((width >= 2 && width <= 8) || (width == 10 || width == 14), "Invalid bit width of OSERDESE2: Valid widths are: 2-8,10,14");
	m_genericParameters["DATA_WIDTH"] = width;		   // Parallel data width (2-8,10,14)
	m_genericParameters["INIT_OQ"] = '0';			 // Initial value of OQ output (1'b0,1'b1)
	m_genericParameters["INIT_TQ"] = '0';			 // Initial value of TQ output (1'b0,1'b1)
	m_genericParameters["SERDES_MODE"] = "MASTER";  // MASTER, SLAVE
	m_genericParameters["SRVAL_OQ"] = '0',			// OQ output value when SR is used (1'b0,1'b1)
	m_genericParameters["SRVAL_TQ"] = '0',			// TQ output value when SR is used (1'b0,1'b1)
	m_genericParameters["TBYTE_CTL"] = "FALSE";	 // Enable tristate byte operation (FALSE, TRUE)
	m_genericParameters["TBYTE_SRC"] = "FALSE";	 // Tristate byte source (FALSE, TRUE)
	m_genericParameters["TRISTATE_WIDTH"] = 1;		// 3-state converter width (1,4)


	m_clockNames = {"CLK", "CLKDIV"};
	m_resetNames = {"", "RST"};
	m_clocks.resize(CLK_COUNT);

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_D1,  "D1");
	declInputBit(IN_D2,  "D2");
	declInputBit(IN_D3,  "D3");
	declInputBit(IN_D4,  "D4");
	declInputBit(IN_D5,  "D5");
	declInputBit(IN_D6,  "D6");
	declInputBit(IN_D7,  "D7");
	declInputBit(IN_D8,  "D8");
	declInputBit(IN_OCE, "OCE");
	declInputBit(IN_SHIFTIN1, "SHIFTIN1");
	declInputBit(IN_SHIFTIN2, "SHIFTIN2");
	declInputBit(IN_T1, "T1");
	declInputBit(IN_T2, "T2");
	declInputBit(IN_T3, "T3");
	declInputBit(IN_T4, "T4");
	declInputBit(IN_TBYTEIN, "TBYTEIN");
	declInputBit(IN_TCE, "TCE");

	declOutputBit(OUT_OFB, "OFB");
	declOutputBit(OUT_OQ, "OQ");
	declOutputBit(OUT_SHIFTOUT1, "SHIFTOUT1");
	declOutputBit(OUT_SHIFTOUT2, "SHIFTOUT2");
	declOutputBit(OUT_TBYTEOUT, "TBYTEOUT");
	declOutputBit(OUT_TFB, "TFB");
	declOutputBit(OUT_TQ, "TQ");
}

void OSERDESE2::setSlave()
{
	m_genericParameters["SERDES_MODE"] = "SLAVE";  // MASTER, SLAVE
}

std::string OSERDESE2::getTypeName() const
{
	return "OSERDESE2";
}

void OSERDESE2::assertValidity() const
{
}

std::unique_ptr<hlim::BaseNode> OSERDESE2::cloneUnconnected() const
{
	OSERDESE2 *ptr;
	std::unique_ptr<BaseNode> res(ptr = new OSERDESE2(0)); // width replaced by copying generic parameters
	copyBaseToClone(res.get());

	return res;
}

std::string OSERDESE2::attemptInferOutputName(size_t outputPort) const
{
	return "OSERDESE2_" + getOutputName(outputPort);
}



}
