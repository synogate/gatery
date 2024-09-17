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
		ila.in("probe0", 1_b) = (BVec)ext(ready(*axi.w.d));
		ila.in("probe1", cfg.addrW) = (BVec)(*axi.w.a)->addr;
		ila.in("probe2", 2_b) = axi.w.b->resp;
		ila.in("probe3", 1_b) = (BVec)ext(valid(axi.w.b));
		ila.in("probe4", 1_b) = (BVec)ext(ready(axi.w.b));
		ila.in("probe5", cfg.addrW) = (BVec)(*axi.r.a)->addr;
		ila.in("probe6", 1_b) = (BVec)ext(ready(axi.r.d));
		ila.in("probe7", 1_b) = (BVec)ext(valid(*axi.w.d));
		ila.in("probe8", 1_b) = (BVec)ext(valid(*axi.r.a));
		ila.in("probe9", 1_b) = (BVec)ext(ready(*axi.r.a));
		ila.in("probe10", cfg.alignedDataW()) = zext(axi.r.d->data, cfg.alignedDataW());
		ila.in("probe11", 1_b) = (BVec)ext(valid(*axi.w.a));
		ila.in("probe12", 1_b) = (BVec)ext(ready(*axi.w.a));
		ila.in("probe13", 2_b) = axi.r.d->resp;
		ila.in("probe14", cfg.alignedDataW()) = zext((*axi.w.d)->data, cfg.alignedDataW());
		ila.in("probe15", cfg.alignedDataW() / 8) = zext((*axi.w.d)->strb, cfg.alignedDataW() / 8);
		ila.in("probe16", 1_b) = (BVec)ext(valid(axi.r.d));
		ila.in("probe17", 3_b) = (*axi.r.a)->prot;
		ila.in("probe18", 3_b) = (*axi.w.a)->prot;
		ila.in("probe19", cfg.idW) = (*axi.w.a)->id;
		ila.in("probe20", cfg.idW) = axi.w.b->id;
		ila.in("probe21", 8_b) = (BVec)(*axi.w.a)->len;
		ila.in("probe22", 1_b) = 0;
		ila.in("probe23", 3_b) = (BVec)(*axi.w.a)->size;
		ila.in("probe24", 2_b) = (*axi.w.a)->burst;
		ila.in("probe25", cfg.idW) = (*axi.r.a)->id;
		ila.in("probe26", 1_b) = 0;
		ila.in("probe27", 8_b) = (BVec)(*axi.r.a)->len;
		ila.in("probe28", 3_b) = (BVec)(*axi.r.a)->size;
		ila.in("probe29", 2_b) = (*axi.r.a)->burst;
		ila.in("probe30", 1_b) = 0;
		ila.in("probe31", 4_b) = (*axi.r.a)->cache;
		ila.in("probe32", 4_b) = (*axi.w.a)->cache;
		ila.in("probe33", 4_b) = (*axi.r.a)->region;
		ila.in("probe34", 4_b) = (BVec)(*axi.r.a)->qos;
		ila.in("probe35", 1_b) = 0;
		ila.in("probe36", 4_b) = (*axi.w.a)->region;
		ila.in("probe37", 4_b) = (BVec)(*axi.w.a)->qos;
		ila.in("probe38", cfg.idW) = axi.r.d->id;
		ila.in("probe39", 1_b) = 0;
		ila.in("probe40", 1_b) = 0; // no wid in axi4
		ila.in("probe41", 1_b) = (BVec)ext(eop(axi.r.d));
		ila.in("probe42", 1_b) = 0;
		ila.in("probe43", 1_b) = (BVec)ext(eop(*axi.w.d));
	}
}
