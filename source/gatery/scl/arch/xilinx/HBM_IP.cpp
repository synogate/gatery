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

#include "HBM_IP.h"
#include <gatery/scl/axi/AxiMemorySimulation.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx
{
	HBM_IP::HBM_IP(std::string_view ipName) :
		ExternalModule(ipName, "xil_defaultlib")
	{
		m_memoryConfig.memoryRegistrationKey = GroupScope::get()->instancePath() + '/' + std::string(ipName) + "/hbm_memory";
		axiMemorySimulationCreateMemory(m_memoryConfig);
	}

	void HBM_IP::clockAPB(const Clock& clk, size_t stackIndex)
	{
		HCL_ASSERT(stackIndex < 2);
		m_controllerClock = clk;

		std::string prefix = "APB_" + std::to_string(stackIndex) + "_";

		clockIn(clk, prefix + "PCLK");

		ClockScope scope(clk);
		HCL_NAMED(m_controllerResetLow);

		in(prefix + "PRESET_N") = m_controllerResetLow;
		m_controllerResetLow = '1';
	}

	void HBM_IP::clockRef(const Clock& clk, size_t stackIndex)
	{
		HCL_ASSERT(stackIndex < 2);
		clockIn(clk, "HBM_REF_CLK_" + std::to_string(stackIndex));
	}

	Axi4 HBM_IP::port(size_t portIndex, BitWidth addrW, bool addECCBitsToData)
	{
		Axi4 axi = Axi4::fromConfig({
			.addrW = addrW,
			.dataW = addECCBitsToData ? 288_b : 256_b,
			.idW = 6_b
		});

		BitWidth hbmAddrOffsetW = addECCBitsToData ? 1_b : 0_b;

		std::string prefix = (boost::format("AXI_%02d_") % portIndex).str();

		// CLOCK
		const Clock& clk = ClockScope::getClk();
		clockIn(clk, prefix + "ACLK");

		// RESET
		Bit resetSignalN = clk.reset(Clock::ResetActive::LOW);
		HCL_NAMED(resetSignalN);
		in(prefix + "ARESET_N") = resetSignalN;
		m_controllerResetLow &= scl::synchronizeRelease(resetSignalN, clk, *m_controllerClock, hlim::RegisterAttributes::Active::LOW);

		// AR
		ready(*axi.ar) = out(prefix + "ARREADY");
		in(prefix + "ARVALID") = valid(*axi.ar);
		in(prefix + "ARADDR", addrW - hbmAddrOffsetW) = zext((BVec)(*axi.ar)->addr.upper(-hbmAddrOffsetW));
		in(prefix + "ARBURST", 2_b) = (BVec)(*axi.ar)->burst;
		in(prefix + "ARID", 6_b) = (*axi.ar)->id;
		in(prefix + "ARLEN", 4_b) = (BVec)(*axi.ar)->len.lower(4_b);
		if (addECCBitsToData)
			in(prefix + "ARSIZE", 3_b) = (BVec)((*axi.ar)->size - 1);
		else
			in(prefix + "ARSIZE", 3_b) = (BVec)(*axi.ar)->size;

		// AW
		ready(*axi.aw) = out(prefix + "AWREADY");
		in(prefix + "AWVALID") = valid(*axi.aw);
		in(prefix + "AWADDR", addrW - hbmAddrOffsetW) = zext((BVec)(*axi.aw)->addr.upper(-hbmAddrOffsetW));
		in(prefix + "AWBURST", 2_b) = (BVec)(*axi.aw)->burst;
		in(prefix + "AWID", 6_b) = (*axi.aw)->id;
		in(prefix + "AWLEN", 4_b) = (BVec)(*axi.aw)->len.lower(4_b);
		if(addECCBitsToData)
			in(prefix + "AWSIZE", 3_b) = (BVec)((*axi.aw)->size - 1);
		else
			in(prefix + "AWSIZE", 3_b) = (BVec)(*axi.aw)->size;

		// W
		ready(*axi.w) = out(prefix + "WREADY");
		in(prefix + "WVALID") = valid(*axi.w);
		in(prefix + "WLAST") = eop(*axi.w);
		in(prefix + "WDATA", 256_b) = (*axi.w)->data.lower(256_b);
		in(prefix + "WSTRB", 32_b) = (*axi.w)->strb.lower(32_b);

		if (addECCBitsToData)
			in(prefix + "WDATA_PARITY", 32_b) = (*axi.w)->data.upper(32_b);
		else
			in(prefix + "WDATA_PARITY", 32_b);

		// R
		in(prefix + "RREADY") = ready(axi.r);
		valid(axi.r) = out(prefix + "RVALID");
		eop(axi.r) = out(prefix + "RLAST");
		axi.r->data = 0;
		axi.r->data.lower(256_b) = out(prefix + "RDATA", 256_b);
		axi.r->resp = out(prefix + "RRESP", 2_b);
		axi.r->id = out(prefix + "RID", 6_b);

		if(addECCBitsToData)
			axi.r->data.upper(32_b) = out(prefix + "RDATA_PARITY", 32_b);

		// B
		in(prefix + "BREADY") = ready(axi.b);
		valid(axi.b) = out(prefix + "BVALID");
		axi.b->resp = out(prefix + "BRESP", 2_b);
		axi.b->id = out(prefix + "BID", 6_b);

		return axiMemorySimulationPortOverride(m_memoryConfig, move(axi));
	}

	Bit HBM_IP::catastrophicTemperature(size_t stackIndex)
	{
		return out("DRAM_" + std::to_string(stackIndex) + "_STAT_CATTRIP");
	}

	UInt HBM_IP::temperature(size_t stackIndex)
	{
		return (UInt)out("DRAM_" + std::to_string(stackIndex) + "_STAT_TEMP", 7_b);
	}

	Bit HBM_IP::abpComplete(size_t stackIndex)
	{
		return out("apb_complete_" + std::to_string(stackIndex));
	}
}
