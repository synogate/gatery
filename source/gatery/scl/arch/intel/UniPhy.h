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
#pragma once
#include <gatery/frontend.h>
#include <gatery/scl/stream/Stream.h>
#include <gatery/scl/tilelink/tilelink.h>

namespace gtry::scl::arch::intel
{
	struct Afi
	{
		BVec addr;
		BVec ba;
		BVec cke;
		BVec cs_n;
		BVec ras_n;
		BVec we_n;
		BVec cas_n;
		BVec rst_n;
		
		BVec odt;
		BVec dqs_burst;
		BVec dm;
		BVec wdata;
		BVec wdata_valid;

		BVec rdata_en;
		BVec rdata_en_full;
		BVec rdata;
		BVec rdata_valid;

		BVec mem_clk_disable;

		Bit init_req;
		Bit cal_req;
		Bit cal_success;
		Bit cal_fail;
		UInt wlat;
		UInt rlat;

		static Afi create(BitWidth addrW, BitWidth dqW, size_t ratio);
		void setDefaults();
	};

	class UniPhy : public ExternalModule
	{
	public:
		UniPhy(BitWidth addrW, BitWidth dqW, std::string_view name);

		void refClk(const Clock& clk);
		Clock afiClk(hlim::ClockRational freq);

		void pin(std::string prefix = "ddr_");
		Afi afi(size_t ratio);

	private:
		BitWidth m_addrW;
		BitWidth m_dqW;

	};

	struct Afi2TileLinkConfig
	{
		BitWidth sourceW = 0_b;
		size_t CL;
		size_t WL;
		picoseconds tRCD;
		picoseconds tPR;
		picoseconds tWR;
		nanoseconds tRFC;
	};

	TileLinkU afi2tilelinkSequentialBridge(Afi& afi, Bit refresh, Afi2TileLinkConfig cfg);
	
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::arch::intel::Afi, addr, ba, cke, cs_n, ras_n, we_n, cas_n, rst_n, odt, dqs_burst, dm, wdata, wdata_valid, rdata_en, rdata_en_full, rdata, rdata_valid, mem_clk_disable, init_req, cal_req, cal_success, cal_fail, wlat, rlat);
