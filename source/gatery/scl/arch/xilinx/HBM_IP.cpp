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

#include "HBM_IP.h"
#include <gatery/scl/axi/AxiMemorySimulation.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx
{
	HBM_IP::HBM_IP(std::string_view ipName) :
		ExternalModule(ipName, "xil_defaultlib")
	{
	}

	void HBM_IP::clockAPB(const Clock& clk, size_t stackIndex)
	{
		HCL_ASSERT(stackIndex < 2);

		std::string prefix = "APB_" + std::to_string(stackIndex) + "_";

		clockIn(clk, prefix + "PCLK");
		in(prefix + "PRESET_N") = '1';
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

		std::string prefix = (boost::format("AXI_%02d_") % portIndex).str();

		// CLOCK
		clockIn(ClockScope::getClk(), prefix + "ACLK");
		in(prefix + "ARESET_N") = '1';

		// AR
		ready(*axi.ar) = out(prefix + "ARREADY");
		valid(*axi.ar) = in(prefix + "ARVALID");
		in(prefix + "ARADDR", addrW) = (BVec)(*axi.ar)->addr;
		in(prefix + "ARBURST", 2_b) = (BVec)(*axi.ar)->burst;
		in(prefix + "ARID", 6_b) = (*axi.ar)->id;
		in(prefix + "ARLEN", 4_b) = (BVec)(*axi.ar)->len.lower(4_b);
		if (addECCBitsToData)
			in(prefix + "ARSIZE", 3_b) = (BVec)((*axi.ar)->size - 1);
		else
			in(prefix + "ARSIZE", 3_b) = (BVec)(*axi.ar)->size;

		// AW
		ready(*axi.aw) = out(prefix + "AWREADY");
		valid(*axi.aw) = in(prefix + "AWVALID");
		in(prefix + "AWADDR", addrW) = (BVec)(*axi.aw)->addr;
		in(prefix + "AWBURST", 2_b) = (BVec)(*axi.aw)->burst;
		in(prefix + "AWID", 6_b) = (*axi.aw)->id;
		in(prefix + "AWLEN", 4_b) = (BVec)(*axi.aw)->len.lower(4_b);
		if(addECCBitsToData)
			in(prefix + "AWSIZE", 3_b) = (BVec)((*axi.aw)->size - 1);
		else
			in(prefix + "AWSIZE", 3_b) = (BVec)(*axi.aw)->size;

		// W
		ready(*axi.w) = out(prefix + "WREADY");
		valid(*axi.w) = in(prefix + "WVALID");
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
		eop(axi.r) = in(prefix + "RLAST");
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

		return axiMemorySimulationOverride({}, move(axi));
	}

	Bit HBM_IP::catastrophicTemperature(size_t stackIndex)
	{
		return Bit();
	}

	UInt HBM_IP::temperature(size_t stackIndex)
	{
		return UInt();
	}

	Bit HBM_IP::abpComplete(size_t stackIndex)
	{
		return Bit();
	}
}
