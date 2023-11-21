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
#include "AxiMemorySimulation.h"

namespace gtry::scl
{
	Axi4& axiMemorySimulation(AxiMemorySimulationConfig cfg)
	{
		Area ent{ "scl_axiMemorySimulation", true };
		std::shared_ptr axi = std::make_shared<Axi4>(Axi4::fromConfig(cfg.axiCfg));

		if(cfg.wordStride.bits() == 0)
			cfg.wordStride = cfg.axiCfg.dataW + cfg.axiCfg.wUserW;

		if(!cfg.storage)
			cfg.storage = std::make_shared<hlim::MemoryStorageDense>((cfg.wordStride + cfg.axiCfg.wUserW).bits() * cfg.axiCfg.addrW.count());
		
		const uint64_t uniqueId = (*axi->ar)->addr.node()->getId();
		pinIn(*axi, "simu_aximem_" + std::to_string(uniqueId), { .simulationOnlyPin = true });

		Clock clock = ClockScope::getClk();
		DesignScope::get()->getCircuit().addSimulationProcess([axi, cfg, clock]() -> SimProcess {
			simu(valid(axi->r)) = '0';
			simu(ready(*axi->ar)) = '1';

			while (true)
			{
				co_await performTransferWait(*axi->ar, clock);

				fork([axi, cfg, clock]() -> SimProcess {
					AxiAddress& ar = **axi->ar;
					uint64_t wordOffset = simu(ar.addr).value() / cfg.axiCfg.dataW.bytes();
					uint64_t size = simu(ar.size).value();
					uint64_t burst = simu(ar.burst).value();
					uint64_t len = simu(ar.len).value() + 1;
					uint64_t id = simu(ar.id).value();

					for (size_t i = 0; i < cfg.readLatency; ++i)
						co_await OnClk(clock);

					for (size_t i = 0; i < len; ++i)
					{
						simu(valid(axi->r)) = '1';
						simu(eop(axi->r)) = i + 1 == len;
						simu(axi->r->id) = id;
						simu(axi->r->resp) = (size_t)AxiResponseCode::OKAY;

						size_t bitOffset = wordOffset * cfg.wordStride.bits();
						simu(axi->r->data) = cfg.storage->read(bitOffset, 8ull << size);
						simu(axi->r->user) = cfg.storage->read(bitOffset + (8ull << size), cfg.axiCfg.rUserW.bits());

						co_await performTransferWait(axi->r, clock);
						
						if (burst == (size_t)AxiBurstType::INCR)
							wordOffset++;
					}

					simu(valid(axi->r)) = '0';
					simu(eop(axi->r)).invalidate();
					simu(axi->r->id).invalidate();
					simu(axi->r->resp).invalidate();
					simu(axi->r->data).invalidate();
					simu(axi->r->user).invalidate();
				});
			}
		});

		DesignScope::get()->getCircuit().addSimulationProcess([axi, cfg, clock]() -> SimProcess {
			simu(valid(axi->b)) = '0';
			simu(ready(*axi->aw)) = '1';
			simu(ready(*axi->w)) = '0';

			while (true)
			{
				co_await performTransferWait(*axi->aw, clock);

				fork([axi, cfg, clock]() -> SimProcess {
					AxiAddress& ar = **axi->aw;
					uint64_t wordOffset = simu(ar.addr).value() / cfg.axiCfg.dataW.bytes();
					//uint64_t size = simu(ar.size).value();
					uint64_t burst = simu(ar.burst).value();
					uint64_t len = simu(ar.len).value() + 1;
					uint64_t id = simu(ar.id).value();

					simu(ready(*axi->w)) = '1';

					BitWidth wordSize = cfg.axiCfg.dataW + cfg.axiCfg.wUserW;
					for (size_t i = 0; i < len; ++i)
					{
						co_await performTransferWait(*axi->w, clock);

						sim::DefaultBitVectorState data = simu((*axi->w)->data);
						sim::DefaultBitVectorState user = simu((*axi->w)->user);
						sim::DefaultBitVectorState strb = simu((*axi->w)->strb);
						sim::DefaultBitVectorState mask;
						mask.resize(data.size());
						for (size_t i = 0; i < strb.size(); ++i)
						{
							size_t symbolSize = data.size() / strb.size();
							mask.setRange(sim::DefaultConfig::VALUE, i * symbolSize, symbolSize, strb.get(sim::DefaultConfig::VALUE, i));
							mask.setRange(sim::DefaultConfig::DEFINED, i * symbolSize, symbolSize, strb.get(sim::DefaultConfig::DEFINED, i));
						}
						cfg.storage->write(wordOffset * wordSize.bits(), data, false, mask);
						cfg.storage->write(wordOffset * wordSize.bits() + data.size(), user, false, {});

						if (burst == (size_t)AxiBurstType::INCR)
							wordOffset++;	
					}
					simu(ready(*axi->w)) = '0';

					simu(axi->b->id) = id;
					simu(axi->b->resp) = (size_t)AxiResponseCode::OKAY;
					co_await performTransfer(axi->b, clock);
					simu(axi->b->id).invalidate();
					simu(axi->b->resp).invalidate();
				});
			}
			});


		HCL_NAMED(*axi);
		return *axi;
	}

	Axi4 axiMemorySimulationOverride(AxiMemorySimulationConfig cfg, Axi4&& axi)
	{
		cfg.axiCfg = axi.config();
		Axi4& simAxi = axiMemorySimulation(cfg);

		Axi4 out;
		out <<= simAxi;

		upstream(out) = upstream(axi);

		ready(*out.ar).exportOverride(ready(*axi.ar));
		ready(*out.aw).exportOverride(ready(*axi.aw));
		ready(*out.w).exportOverride(ready(*axi.w));

		valid(out.r).exportOverride(valid(axi.r));
		out.r->id.exportOverride(axi.r->id);
		out.r->data.exportOverride(axi.r->data);
		out.r->resp.exportOverride(axi.r->resp);
		out.r->user.exportOverride(axi.r->user);

		valid(out.b).exportOverride(valid(axi.b));
		out.b->id.exportOverride(axi.b->id);
		out.b->resp.exportOverride(axi.b->resp);

		return out;
	}
}
