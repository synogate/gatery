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
#include "BRAM.h"

using namespace std::literals;

namespace gtry::scl::arch::colognechip
{
	CC_BRAM::CC_BRAM(std::string_view name, BitWidth portW, BitWidth addrW) :
		ExternalModule("CC_BRAM_40K"),
		m_portW(portW),
		m_addrW(addrW)
	{
		requiresComponentDeclaration(true);
		isEntity(false);

		generic("RAM_MODE") = "TDP";
		generic("LOC") = "UNPLACED";

		for (std::string p : {"A_"s, "B_"s})
		{
			generic(p + "WR_MODE") = "NO_CHANGE";
			generic(p + "RD_WIDTH") = 0;
			generic(p + "WR_WIDTH") = 0;
			generic(p + "CLK_INV") = false;
			generic(p + "EN_INV") = false;
			generic(p + "WE_INV") = false;
			generic(p + "DO_REG") = false;
			in(p + "CLK") = '0';
			in(p + "ADDR", 16_b) = 0;
			in(p + "EN") = '0';
			in(p + "WE") = '0';
			in(p + "DI", portW) = 0;
			in(p + "BM", portW) = 0;
			out(p + "DO", portW);
		}
	}

	void CC_BRAM::address(Port port, UInt address, Bit enable)
	{
		HCL_DESIGNCHECK(address.width() <= m_addrW);

		generic(pname(port, "CLK_INV")) = ClockScope::getClk().getClk()->getTriggerEvent() == Clock::TriggerEvent::FALLING;
		in(pname(port, "CLK")) = ClockScope::getClk().clkSignal();
		in(pname(port, "EN")) = enable;
		
		UInt addrExt = zext(address, std::max(address.width(), 9_b));
		BVec& addrPort = in(pname(port, "ADDR"), 16_b);
		addrPort = 0;
		addrPort.upper(addrExt.width()) = addrExt;

		BitWidth dataW{ 1ull << (m_addrW - addrExt.width()).bits() };
		dataW = dataW + dataW.bits() / 4;
		if (dataW > m_portW)
			dataW = m_portW;
		m_dataW[port] = dataW;
	}

	BVec CC_BRAM::read(Port port)
	{
		HCL_DESIGNCHECK_HINT(m_dataW[port] != 0_b, "assign address first");
		generic(pname(port, "RD_WIDTH")) = m_dataW[port].bits();
		return out(pname(port, "DO"), m_portW).lower(m_dataW[port]);
	}

	void CC_BRAM::write(Port port, Bit writeEnable, BVec data, BVec mask)
	{
		HCL_DESIGNCHECK_HINT(m_dataW[port] != 0_b, "assign address first");
		HCL_DESIGNCHECK_HINT(data.width() != m_portW * 2, "SDP mode not implemented");
		HCL_DESIGNCHECK_HINT(data.width() <= m_dataW[port], "data width is more than supported");
		HCL_DESIGNCHECK_HINT(data.width() == mask.width(), "mask is per bit");

		generic(pname(port, "WR_WIDTH")) = m_dataW[port].bits();
		in(pname(port, "WE")) = writeEnable;
		in(pname(port, "DI"), m_portW) = zext(data, m_portW);
		in(pname(port, "BM"), m_portW) = zext(mask, m_portW);
	}

	std::string CC_BRAM::pname(Port port, std::string suffix)
	{
		return (port == A ? "A_" : "B_") + suffix;
	}

	CC_BRAM_40K::CC_BRAM_40K() :
		CC_BRAM("CC_BRAM_40K", 40_b, 15_b)
	{
		generic("A_ECC_EN") = false;
		out("A_ECC_1B_ERR");
		out("A_ECC_1B_ERR");

		generic("B_ECC_EN") = false;
		out("B_ECC_2B_ERR");
		out("B_ECC_2B_ERR");
	}

}
