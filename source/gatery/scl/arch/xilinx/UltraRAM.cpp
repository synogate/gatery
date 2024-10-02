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
#include "UltraRAM.h"
#include <gatery/debug/DebugInterface.h>

using namespace gtry;
using namespace gtry::scl;

static void exportOverrideTileLink(TileLinkUL& tl, TileLinkUL& phys)
{
	downstream(phys) = downstream(tl);

	ready(tl.a).exportOverride(ready(phys.a));

	valid(*tl.d).exportOverride(valid(*phys.d));
	(*tl.d)->opcode.exportOverride((*phys.d)->opcode);
	(*tl.d)->param.exportOverride((*phys.d)->param);
	(*tl.d)->size.exportOverride((*phys.d)->size);
	(*tl.d)->source.exportOverride((*phys.d)->source);
	(*tl.d)->sink.exportOverride((*phys.d)->sink);
	(*tl.d)->data.exportOverride((*phys.d)->data);
	(*tl.d)->error.exportOverride((*phys.d)->error);
}

std::array<TileLinkUL, 2> arch::xilinx::ultraRam(size_t numWords, UltraRamSettings&& cfg)
{
	Area ent{ "scl_ultraRam", true };
	if (!cfg.name.empty())
		ent.instanceName(std::string{ cfg.name });

	size_t numRamBlocks = (numWords + 4095) / 4096;
	if (!cfg.latency)
	{
		// input register, output register, last cascade register and one extra register for each clock region crossing
		cfg.latency = 2 + 1 + numRamBlocks / 7 + numRamBlocks / 16;
	}
	HCL_DESIGNCHECK_HINT(*cfg.latency != 0, "Latency must be at least 1 cycle");

	std::vector<URAM288> ram(numRamBlocks);

	if (*cfg.latency > ram.size() + 2)
	{
		dbg::log(dbg::LogMessage(ent.getNodeGroup()) << dbg::LogMessage::LOG_WARNING << dbg::LogMessage::LOG_DESIGN << "Thre requested URAM latency of " << *cfg.latency << " cannot be fullfilled with " << ram.size() << " memory blocks.");
		*cfg.latency = ram.size() + 2;
	}

	size_t pipeLineRegisterSteps = 0;
	if(*cfg.latency > 3)
	{
		// -1 for the last ram that is always registered
		pipeLineRegisterSteps = (ram.size() - 1) / (*cfg.latency - 3);
	}

	for (size_t i = 0; i < ram.size(); ++i)
	{
		ram[i].clock(ClockScope::getClk());
		ram[i].enableOutputRegister(URAM288::A, *cfg.latency >= 2);
		ram[i].enableOutputRegister(URAM288::B, *cfg.latency >= 2);

		if (i > 0)
			ram[i].cascade(ram[i - 1], numRamBlocks);
		if(pipeLineRegisterSteps)
			ram[i].cascadeReg(i % pipeLineRegisterSteps == pipeLineRegisterSteps - 1);
	}

	if (*cfg.latency > 2)
		ram.back().cascadeReg(true);

	std::array<TileLinkUL, 2> outPhys{
		ultraRamPort(BitWidth::count(numWords), ram.front(), ram.back(), URAM288::A, cfg.aSourceW, *cfg.latency),
		ultraRamPort(BitWidth::count(numWords), ram.front(), ram.back(), URAM288::B, cfg.bSourceW, *cfg.latency)
	};

	// simulation version
	Memory<BVec> simMem(numWords, 64_b);
	simMem.setType(MemType::DONT_CARE, 0);
	
	std::array<TileLinkUL, 2> outSim;
	for (size_t i = 0; i < outSim.size(); ++i)
	{
		tileLinkInit(outSim[i], BitWidth::count(numWords) + 3_b, 64_b, 2_b, i ? cfg.bSourceW : cfg.aSourceW);
//		ready(*outSim[i].d) = '1'; // read(.a) is wired to read(.d) so setting this to '1' is breaking things
		connect(simMem, outSim[i]);

		for (size_t j = 0; j < *cfg.latency; ++j)
			*outSim[i].d = scl::strm::regDownstreamBlocking(move(*outSim[i].d));

		exportOverrideTileLink(outSim[i], outPhys[i]);
	}

	setName(outSim, "out");
	return outSim;
}

TileLinkUL gtry::scl::arch::xilinx::ultraRamPort(BitWidth addrW, URAM288& inRam, URAM288& outRam, URAM288::Port port, BitWidth sourceW, size_t latency)
{
	TileLinkUL out = tileLinkInit<TileLinkUL>(addrW + 3_b, 64_b, sourceW);

	inRam.port(port, {
		.din = zext(out.a->data),
		.addr = zext(out.a->address.upper(-3_b)),
		.en = valid(out.a),
		.rdb_wr = out.a->isPut(),
		.bwe = zext(out.a->mask),
	});
	ready(out.a) = ready(*out.d);
	sim_assert(ready(*out.d) | !valid(*out.d)) << "URAM does not support back pressure in cascade mode";

	RvStream<TileLinkD> tlResult = { tileLinkDefaultResponse(*out.a) };
	valid(tlResult) = valid(out.a);
	for (size_t i = 0; i < latency; ++i)
		tlResult = regDownstreamBlocking(move(tlResult));
	*out.d <<= tlResult;

	URAM288::PortOut memResult = outRam.port(port);
	(*out.d)->data = memResult.dout.lower(64_b);
	return out;
}
