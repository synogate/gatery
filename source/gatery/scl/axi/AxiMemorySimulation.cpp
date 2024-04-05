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
#include "AxiMemorySimulation.h"

#include <gatery/scl/stream/SimuHelpers.h>

namespace gtry::scl
{
	void axiMemorySimulationCreateMemory(AxiMemorySimulationConfig cfg)
	{
		DesignScope::get()->getCircuit().addSimulationProcess([cfg]() -> SimProcess {
			BitWidth width;
			if (cfg.memorySize)
				width = *cfg.memorySize;
			else
				width = cfg.wordStride * cfg.axiCfg.wordAddrW().count();
			
			emplaceSimData<hlim::MemoryStorageSparse>(cfg.memoryRegistrationKey, width.bits(), cfg.initialization);
			co_return;
		});
	}

	Axi4& axiMemorySimulationPort(AxiMemorySimulationConfig cfg)
	{
		Area ent{ "scl_axiMemorySimulation", true };
		std::shared_ptr axi = std::make_shared<Axi4>(Axi4::fromConfig(cfg.axiCfg));

		if(cfg.wordStride.bits() == 0)
			cfg.wordStride = cfg.axiCfg.dataW + cfg.axiCfg.wUserW;

		const uint64_t uniqueId = (*axi->ar)->addr.node()->getId();
		pinIn(*axi, "simu_aximem_" + std::to_string(uniqueId), { .simulationOnlyPin = true });

		Clock clock = ClockScope::getClk();
		DesignScope::get()->getCircuit().addSimulationProcess([axi, cfg, clock]() -> SimProcess {

			auto &storage = getSimData<hlim::MemoryStorageSparse>(cfg.memoryRegistrationKey);

			auto axiCfg = cfg.axiCfg;
			auto wordStride = cfg.wordStride;
			auto readLatency = cfg.readLatency;


			fork([axi, clock, &storage, wordStride, axiCfg, readLatency]() -> SimProcess {
				simu(valid(axi->r)) = '0';
				simu(ready(*axi->ar)) = '1';

				std::mt19937 rng(13579);
				std::bernoulli_distribution memoryDelayed(0.01);
				std::uniform_int_distribution<size_t> randomDelayAmount(1, 16);

				size_t slot_next = 0;
				size_t slot_current = 0;
				while (true)
				{
					co_await performTransferWait(*axi->ar, clock);

					fork([axi, clock, slot_next, &slot_current, &storage, wordStride, axiCfg, readLatency, &rng, &randomDelayAmount, &memoryDelayed]() -> SimProcess {
						AxiAddress& ar = **axi->ar;
						uint64_t wordOffset = simu(ar.addr).value() / axiCfg.alignedDataW().bytes();
						//uint64_t size = simu(ar.size).value();
						uint64_t burst = simu(ar.burst).value();
						uint64_t len = simu(ar.len).value() + 1;
						uint64_t id = simu(ar.id).value();

						for (size_t i = 0; i < readLatency; ++i)
							co_await OnClk(clock);

						while(slot_next != slot_current)
							co_await OnClk(clock);

						if (memoryDelayed(rng)) {
							size_t amount = randomDelayAmount(rng);

							for (size_t i = 0; i < amount; ++i)
								co_await OnClk(clock);
						}

						for (size_t i = 0; i < len; ++i)
						{
							simu(valid(axi->r)) = '1';
							simu(eop(axi->r)) = i + 1 == len;
							simu(axi->r->id) = id;
							simu(axi->r->resp) = (size_t)AxiResponseCode::OKAY;

							size_t bitOffset = wordOffset * wordStride.bits();
							simu(axi->r->data) = storage.read(bitOffset, axiCfg.dataW.bits());
							simu(axi->r->user) = storage.read(bitOffset + axiCfg.dataW.bits(), axiCfg.rUserW.bits());

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

						slot_current++;
					});

					slot_next++;
					while (slot_next - slot_current > 8)
					{
						simu(ready(*axi->ar)) = '0';
						co_await OnClk(clock);
						simu(ready(*axi->ar)) = '1';
					}
				}
			});

			fork([axi, clock, &storage, wordStride, axiCfg]() -> SimProcess {
				simu(valid(axi->b)) = '0';
				simu(ready(*axi->aw)) = '1';
				simu(ready(*axi->w)) = '0';

				size_t slot_next = 0;
				size_t slot_current = 0;
				size_t slot_current_ack = 0;
				while (true)
				{
					co_await performTransferWait(*axi->aw, clock);

					fork([axi, clock, slot_next, &slot_current, &slot_current_ack, &storage, wordStride, axiCfg]() -> SimProcess {
						AxiAddress& ar = **axi->aw;
						uint64_t wordOffset = simu(ar.addr).value() / axiCfg.alignedDataW().bytes();
						//uint64_t size = simu(ar.size).value();
						uint64_t burst = simu(ar.burst).value();
						uint64_t len = simu(ar.len).value() + 1;
						uint64_t id = simu(ar.id).value();

						while (slot_next != slot_current)
							co_await OnClk(clock);

						simu(ready(*axi->w)) = '1';

						BitWidth wordSize = axiCfg.dataW + axiCfg.wUserW;
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
							storage.write(wordOffset * wordSize.bits(), data, false, mask);
							storage.write(wordOffset * wordSize.bits() + data.size(), user, false, {});

							if (burst == (size_t)AxiBurstType::INCR)
								wordOffset++;	
						}
						simu(ready(*axi->w)) = '0';
						slot_current++;

						while (slot_next != slot_current_ack)
							co_await OnClk(clock);

						simu(axi->b->id) = id;
						simu(axi->b->resp) = (size_t)AxiResponseCode::OKAY;
						co_await performTransfer(axi->b, clock);
						simu(axi->b->id).invalidate();
						simu(axi->b->resp).invalidate();
						slot_current_ack++;
					});

					slot_next++;
					while (slot_next - slot_current > 8)
					{
						simu(ready(*axi->aw)) = '0';
						co_await OnClk(clock);
						simu(ready(*axi->aw)) = '1';
					}
				}
			});

			co_return;
		});

		setName(*axi, "axi");
		return *axi;
	}

	Axi4 axiMemorySimulationPortOverride( AxiMemorySimulationConfig cfg, Axi4&& axi)
	{
		cfg.axiCfg = axi.config();
		Axi4& simAxi = axiMemorySimulationPort(cfg);

		Axi4 out = constructFrom(axi);
		out <<= simAxi;

		upstream(axi) = upstream(out);

		ready(*out.ar).exportOverride(ready(*axi.ar));
		ready(*out.aw).exportOverride(ready(*axi.aw));
		ready(*out.w).exportOverride(ready(*axi.w));

		valid(out.r).exportOverride(valid(axi.r));
		eop(out.r).exportOverride(eop(axi.r));
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
