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
#include <gatery/scl/Counter.h>
#include <gatery/scl/ShiftReg.h>

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

	BVec simReadValid = ret.rdata_en;
	for (size_t i = 0; i < 10; ++i)
		simReadValid = reg(simReadValid, ConstBVec(0, simReadValid.width()));
	ret.rdata_valid = simOverride(out(pre + "rdata_valid", ret.rdata_valid.width()), simReadValid);

	ret.cal_success = simOverride(out(pre + "cal_success"), '1');
	ret.cal_fail = simOverride(out(pre + "cal_fail"), '0');
	ret.wlat = simOverride((UInt)out(pre + "wlat", ret.wlat.width()), 3);
	ret.rlat = simOverride((UInt)out(pre + "rlat", ret.rlat.width()), 0); // for some reason rlat is 0 in practice

	setName(ret.wlat, "test_wlat");

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
		.wdata_valid = dqW / 8 * ratio,
		
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


gtry::scl::TileLinkU gtry::scl::arch::intel::afi2tilelinkSequentialBridge(Afi& afi, Bit refresh, Afi2TileLinkConfig cfg)
{
	Area ent{ "scl_afi2avalonSequentialBridge", true };
	HCL_NAMED(afi);
	HCL_NAMED(refresh);

	picoseconds tCK = ClockScope::getClk().period() / 2;
	BitWidth addrW = afi.addr.width() / 2;
	BitWidth baW = afi.ba.width() / 2;
	BitWidth dataW = afi.rdata.width();
	BitWidth colSelW = 10_b - BitWidth::count(dataW.bytes() / 2);

	auto ret = tileLinkInit<TileLinkU>(addrW + baW + colSelW, dataW, cfg.sourceW);
	**ret.d = tileLinkDefaultResponse(*ret.a);
	(**ret.d).data = afi.rdata;

	ready(ret.a) = '0';
	valid(*ret.d) = '0';
	(**ret.d).data = afi.rdata;

	afi.setDefaults();
	
	// row command always in first slot
	afi.ba.part(2, 0) = ret.a->wordAddress().upper(baW);
	afi.addr.part(2, 0) = ret.a->wordAddress()(colSelW.bits(), addrW); // row address

	// col command always in second slot
	afi.ba.part(2, 1) = ret.a->wordAddress().upper(baW);
	afi.addr.part(2, 1) = zext(cat(
		'0', // A12 BC4 burst lenth
		'0', // A11 used in 4bit dqs group versions only
		'1', // A10 auto precharge
		ret.a->wordAddress().lower(colSelW), // column address
		ConstBVec(0, BitWidth{ 10 - colSelW.bits() }) // burst wrap offset
	));

	afi.wdata = ret.a->data;
	afi.dm = ~ret.a->mask;

	enum class AfiState
	{
		calibration,
		idle,
		refresh,

		cas,

		write_wait_wlat,
		write_data,
		write_commit,
		write_precharge,

		read_commit,
		read_precharge,
	};
	Reg<Enum<AfiState>> state{ AfiState::calibration };
	state.setName("state");

	IF(state.current() == AfiState::calibration)
	{
		IF(afi.cal_success)
			state = AfiState::idle;
	}

	scl::Counter timer(cfg.tRFC);
	IF(!timer.isLast())
		timer.inc();

	Bit refreshRequest = flag(refresh, state.current() == AfiState::refresh) | name("refreshRequest");
	IF(state.current() == AfiState::idle)
	{
		timer.reset();

		IF(refreshRequest)
			state = AfiState::refresh;
		ELSE IF(valid(ret.a))
		{
			afi.ras_n = 0;
			afi.cs_n[0] = '0';
			state = AfiState::cas;
		}
	}

	IF(state.current() == AfiState::cas)
	{
		// - 1 for the extra delay between command slot 0 (RAS) and 1 (CAS)
		size_t cycles = ceil((ClockRational{ cfg.tRCD } / tCK - 1) / 2);
		IF(timer.value().lower(3_b) == cycles - 1)
		{
			afi.cas_n = 0;
			afi.cs_n[1] = '0';
			timer.reset();

			IF(ret.a->isPut())
			{
				afi.we_n = 0;
				state = AfiState::write_wait_wlat;
			}
			ELSE
			{
				afi.rdata_en = 3;
				afi.rdata_en_full = 3;
				state = AfiState::read_commit;
			}
		}
	}

	IF(state.current() == AfiState::write_wait_wlat)
	{
		// Note that with WL 6 on MAX10 we had to remove the -1 to make it work. What might be caused by tCK (max) violation.
		IF(zext(timer.value()) == afi.wlat - 1)
		{
			afi.dqs_burst.part(2, 1) = afi.dqs_burst.part(2, 1).width().mask(); // write preamble
			state = AfiState::write_data;
		}
	}

	IF(state.current() == AfiState::write_data)
	{
		afi.dqs_burst = afi.dqs_burst.width().mask();
		afi.wdata_valid = afi.wdata_valid.width().mask();
		state = AfiState::write_commit;
	}

	IF(state.current() == AfiState::write_commit)
	{
		valid(*ret.d) = '1';
		ready(ret.a) = ready(*ret.d);

		IF(transfer(ret.a))
			state = AfiState::write_precharge;
	}

	IF(state.combinatorial() == AfiState::write_precharge)
	{
		size_t writeCycles = (cfg.WL + 4 /* write data phase */ + ceil(ClockRational(cfg.tWR + cfg.tPR) / tCK) + 1) / 2;
		IF(timer.value() >= writeCycles - 1)
			state = AfiState::idle;
	}

	IF(state.current() == AfiState::read_commit)
	{
		Bit done = afi.rdata_valid != 0 | timer.isLast();
		(**ret.d).error = afi.rdata_valid == 0;
		valid(*ret.d) = done;
		ready(ret.a) = done;

		IF(done)
		{
			state = AfiState::read_precharge;
			sim_assert(ready(*ret.d)) << "no back pressure on read data supported from external memory";
		}
	}

	IF(state.combinatorial() == AfiState::read_precharge)
	{
		size_t readCycles = (cfg.CL + 3 /* read data phase */ + ceil(ClockRational{ cfg.tPR } / tCK) + 1) / 2;
		IF(timer.value() >= readCycles - 1)
			state = AfiState::idle;
	}

	IF(state.current() != AfiState::refresh & state.next() == AfiState::refresh)
	{
		// REFRESH command
		afi.ras_n = 0;
		afi.cas_n = 0;
		afi.cs_n[1] = '0';
	}

	IF(state.current() == AfiState::refresh)
	{
		IF(timer.isLast())
			state = AfiState::idle;
	}

	HCL_NAMED(ret);
	return ret;
}
