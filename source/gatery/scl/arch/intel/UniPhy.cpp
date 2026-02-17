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
#include "UniPhy.h"

using namespace gtry::scl::arch::intel;

UniPhy::UniPhy(BitWidth addrW, BitWidth dqW, std::string_view name) :
	ExternalModule(name),
	m_dqW(dqW),
	m_addrW(addrW)
{
	requiresComponentDeclaration(true);
	isEntity(false);
}

void UniPhy::refClk(const Clock& clk)
{
	const auto& rstAttr = clk.getClk()->getRegAttribs();
	if (rstAttr.resetType == hlim::RegisterAttributes::ResetType::NONE)
	{
		in("global_reset_n") = '1';
		clockIn(clk, "pll_ref_clk");
	}
	else
	{
		HCL_DESIGNCHECK(rstAttr.resetActive == hlim::RegisterAttributes::Active::LOW);
		clockIn(clk, "pll_ref_clk", "global_reset_n");
	}
	
	in("soft_reset_n") = '1';
}

gtry::Clock UniPhy::afiClk(hlim::ClockRational freq)
{
	return clockOut("afi_clk", "afi_reset_n", ClockConfig{
		.absoluteFrequency = freq,
		.resetType = ClockConfig::ResetType::ASYNCHRONOUS,
		.resetActive = ClockConfig::ResetActive::LOW,
	});
}

void UniPhy::pin(std::string prefix)
{
	pinOut(out("mem_a", m_addrW), prefix + "a");
	pinOut(out("mem_ba", 3_b), prefix + "ba");
	inoutPin("mem_ck", prefix + "ck_p", 1_b);
	inoutPin("mem_ck_n", prefix + "ck_n", 1_b);
	pinOut(out("mem_cke", 1_b), prefix + "cke");
	pinOut(out("mem_cs_n", 1_b), prefix + "cs_n");
	pinOut(out("mem_dm", m_dqW / 8), prefix + "dm");
	pinOut(out("mem_ras_n", 1_b), prefix + "ras_n");
	pinOut(out("mem_cas_n", 1_b), prefix + "cas_n");
	pinOut(out("mem_we_n", 1_b), prefix + "we_n");
	pinOut(out("mem_reset_n"), prefix + "reset_n");
	inoutPin("mem_dq", prefix + "dq", m_dqW);
	inoutPin("mem_dqs", prefix + "dqs_p", m_dqW / 8);
	inoutPin("mem_dqs_n", prefix + "dqs_n", m_dqW / 8);
	pinOut(out("mem_odt"), prefix + "odt");
}

Afi UniPhy::afi(size_t ratio)
{
	Afi ret = Afi::create(m_addrW, m_dqW, ratio);

	std::string pre = "afi_";
	in(pre + "addr", ret.addr.width()) = ret.addr;
	in(pre + "ba", ret.ba.width()) = ret.ba;
	in(pre + "cke", ret.cke.width()) = ret.cke;
	in(pre + "cs_n", ret.cs_n.width()) = ret.cs_n;
	in(pre + "ras_n", ret.ras_n.width()) = ret.ras_n;
	in(pre + "we_n", ret.we_n.width()) = ret.we_n;
	in(pre + "cas_n", ret.cas_n.width()) = ret.cas_n;
	in(pre + "rst_n", ret.rst_n.width()) = ret.rst_n;
	in(pre + "odt", ret.odt.width()) = ret.odt;
	in(pre + "dqs_burst", ret.dqs_burst.width()) = ret.dqs_burst;
	in(pre + "dm", ret.dm.width()) = ret.dm;
	in(pre + "wdata", ret.wdata.width()) = ret.wdata;
	in(pre + "wdata_valid", ret.wdata_valid.width()) = ret.wdata_valid;
	in(pre + "odt", ret.odt.width()) = ret.odt;
	in(pre + "rdata_en", ret.rdata_en.width()) = ret.rdata_en;
	in(pre + "rdata_en_full", ret.rdata_en_full.width()) = ret.rdata_en_full;
	in(pre + "init_req") = ret.init_req;
	in(pre + "cal_req") = ret.cal_req;

	ret.rdata = out(pre + "rdata", ret.rdata.width());
	ret.rdata_valid = out(pre + "rdata_valid", ret.rdata_valid.width());

	ret.cal_success = out(pre + "cal_success");
	ret.cal_fail = out(pre + "cal_fail");
	ret.wlat = (UInt)out(pre + "wlat", ret.wlat.width());
	ret.rlat = (UInt)out(pre + "rlat", ret.rlat.width());

	return ret;
}

Afi Afi::create(BitWidth addrW, BitWidth dqW, size_t ratio)
{
	return {
		.addr = addrW * ratio,
		.ba = 3_b * ratio,
		.cke = BitWidth{ratio},
		.cs_n = BitWidth{ratio},
		.ras_n = BitWidth{ratio},
		.we_n = BitWidth{ratio},
		.cas_n = BitWidth{ratio},
		.rst_n = BitWidth{ratio},
		
		.odt = BitWidth{ratio},
		.dqs_burst = dqW / 8 * ratio,
		.dm = dqW / 8 * ratio * 2,
		.wdata = dqW * ratio * 2,
		.wdata_valid = BitWidth{ratio},
		
		.rdata_en = BitWidth{ratio},
		.rdata_en_full = BitWidth{ratio},
		.rdata = dqW * ratio * 2,
		.rdata_valid = BitWidth{ratio},

		.mem_clk_disable = BitWidth{1},

		.wlat = 6_b,
		.rlat = 6_b,
	};
}

void Afi::setDefaults()
{
	addr = 0;
	ba = 0;

	cke = cke.width().mask();
	cs_n = cs_n.width().mask();
	ras_n = ras_n.width().mask();
	we_n = we_n.width().mask();
	cas_n = cas_n.width().mask();
	rst_n = rst_n.width().mask();

	odt = 0;
	dqs_burst = 0;
	dm = 0;
	wdata = 0;
	wdata_valid = 0;

	rdata_en = 0;
	rdata_en_full = 0;
	
	mem_clk_disable = 0;

	init_req = '0';
	cal_req = '0';
}
