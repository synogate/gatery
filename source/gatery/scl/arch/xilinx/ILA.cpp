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
#include "ILA.h"

#include <gatery/scl/axi/axi.h>

namespace gtry::scl::arch::xilinx
{
	void ila(const Axi4& axi, std::string_view generatedIpName)
	{
		ExternalModule ila(generatedIpName, "xil_defaultlib");
		ila.hasSideEffects(true);
		ila.clockIn("clk");

		const AxiConfig cfg = axi.config();

		// the mapping is from ila_0.vho:
		ila.in("probe0", 1_b) = (BVec)ext(ready(*axi.w));
		ila.in("probe1", cfg.addrW) = (BVec)(*axi.aw)->addr;
		ila.in("probe2", 2_b) = axi.b->resp;
		ila.in("probe3", 1_b) = (BVec)ext(valid(axi.b));
		ila.in("probe4", 1_b) = (BVec)ext(ready(axi.b));
		ila.in("probe5", cfg.addrW) = (BVec)(*axi.ar)->addr;
		ila.in("probe6", 1_b) = (BVec)ext(ready(axi.r));
		ila.in("probe7", 1_b) = (BVec)ext(valid(*axi.w));
		ila.in("probe8", 1_b) = (BVec)ext(valid(*axi.ar));
		ila.in("probe9", 1_b) = (BVec)ext(ready(*axi.ar));
		ila.in("probe10", cfg.alignedDataW()) = zext(axi.r->data, cfg.alignedDataW());
		ila.in("probe11", 1_b) = (BVec)ext(valid(*axi.aw));
		ila.in("probe12", 1_b) = (BVec)ext(ready(*axi.aw));
		ila.in("probe13", 2_b) = axi.r->resp;
		ila.in("probe14", cfg.alignedDataW()) = zext((*axi.w)->data, cfg.alignedDataW());
		ila.in("probe15", cfg.alignedDataW() / 8) = zext((*axi.w)->strb, cfg.alignedDataW() / 8);
		ila.in("probe16", 1_b) = (BVec)ext(valid(axi.r));
		ila.in("probe17", 3_b) = (*axi.ar)->prot;
		ila.in("probe18", 3_b) = (*axi.aw)->prot;
		ila.in("probe19", cfg.idW) = (*axi.aw)->id;
		ila.in("probe20", cfg.idW) = axi.b->id;
		ila.in("probe21", 8_b) = (BVec)(*axi.aw)->len;
		ila.in("probe22", 1_b) = 0;
		ila.in("probe23", 3_b) = (BVec)(*axi.aw)->size;
		ila.in("probe24", 2_b) = (*axi.aw)->burst;
		ila.in("probe25", cfg.idW) = (*axi.ar)->id;
		ila.in("probe26", 1_b) = 0;
		ila.in("probe27", 8_b) = (BVec)(*axi.ar)->len;
		ila.in("probe28", 3_b) = (BVec)(*axi.ar)->size;
		ila.in("probe29", 2_b) = (*axi.ar)->burst;
		ila.in("probe30", 1_b) = 0;
		ila.in("probe31", 4_b) = (*axi.ar)->cache;
		ila.in("probe32", 4_b) = (*axi.aw)->cache;
		ila.in("probe33", 4_b) = (*axi.ar)->region;
		ila.in("probe34", 4_b) = (BVec)(*axi.ar)->qos;
		ila.in("probe35", 1_b) = 0;
		ila.in("probe36", 4_b) = (*axi.aw)->region;
		ila.in("probe37", 4_b) = (BVec)(*axi.aw)->qos;
		ila.in("probe38", cfg.idW) = axi.r->id;
		ila.in("probe39", 1_b) = 0;
		ila.in("probe40", 1_b) = 0; // no wid in axi4
		ila.in("probe41", 1_b) = (BVec)ext(eop(axi.r));
		ila.in("probe42", 1_b) = 0;
		ila.in("probe43", 1_b) = (BVec)ext(eop(*axi.w));
	}
}
