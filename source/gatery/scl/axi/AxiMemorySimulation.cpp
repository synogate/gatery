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

		const uint64_t uniqueId = (*axi->r.a)->addr.node()->getId();
		pinIn(*axi, "simu_aximem_" + std::to_string(uniqueId), { .simulationOnlyPin = true });

		Clock clock = ClockScope::getClk();
		DesignScope::get()->getCircuit().addSimulationProcess([axi, cfg, clock]() -> SimProcess {

			auto &storage = getSimData<hlim::MemoryStorageSparse>(cfg.memoryRegistrationKey);

			auto axiCfg = cfg.axiCfg;
			auto wordStride = cfg.wordStride;
			auto readLatency = cfg.readLatency;


			fork([axi, clock, &storage, wordStride, axiCfg, readLatency]() -> SimProcess {
				simu(valid(axi->r.d)) = '0';
				simu(ready(*axi->r.a)) = '1';

				std::mt19937 rng(13579);
				std::bernoulli_distribution memoryDelayed(0.01);
				std::uniform_int_distribution<size_t> randomDelayAmount(1, 16);

				size_t slot_next = 0;
				size_t slot_current = 0;
				while (true)
				{
					co_await performTransferWait(*axi->r.a, clock);

					fork([axi, clock, slot_next, &slot_current, &storage, wordStride, axiCfg, readLatency, &rng, &randomDelayAmount, &memoryDelayed]() -> SimProcess {
						AxiAddress& ar = **axi->r.a;
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
							simu(valid(axi->r.d)) = '1';
							simu(eop(axi->r.d)) = i + 1 == len;
							simu(axi->r.d->id) = id;
							simu(axi->r.d->resp) = (size_t)AxiResponseCode::OKAY;

							size_t bitOffset = wordOffset * wordStride.bits();
							simu(axi->r.d->data) = storage.read(bitOffset, axiCfg.dataW.bits());
							simu(axi->r.d->user) = storage.read(bitOffset + axiCfg.dataW.bits(), axiCfg.rUserW.bits());

							co_await performTransferWait(axi->r.d, clock);
							
							if (burst == (size_t)AxiBurstType::INCR)
								wordOffset++;
						}

						simu(valid(axi->r.d)) = '0';
						simu(eop(axi->r.d)).invalidate();
						simu(axi->r.d->id).invalidate();
						simu(axi->r.d->resp).invalidate();
						simu(axi->r.d->data).invalidate();
						simu(axi->r.d->user).invalidate();

						slot_current++;
					});

					slot_next++;
					while (slot_next - slot_current > 8)
					{
						simu(ready(*axi->r.a)) = '0';
						co_await OnClk(clock);
						simu(ready(*axi->r.a)) = '1';
					}
				}
			});

			fork([axi, clock, &storage, wordStride, axiCfg]() -> SimProcess {
				simu(valid(axi->w.b)) = '0';
				simu(ready(*axi->w.a)) = '1';
				simu(ready(*axi->w.d)) = '0';

				size_t slot_next = 0;
				size_t slot_current = 0;
				size_t slot_current_ack = 0;
				while (true)
				{
					co_await performTransferWait(*axi->w.a, clock);

					fork([axi, clock, slot_next, &slot_current, &slot_current_ack, &storage, wordStride, axiCfg]() -> SimProcess {
						AxiAddress& ar = **axi->w.a;
						uint64_t wordOffset = simu(ar.addr).value() / axiCfg.alignedDataW().bytes();
						//uint64_t size = simu(ar.size).value();
						uint64_t burst = simu(ar.burst).value();
						uint64_t len = simu(ar.len).value() + 1;
						uint64_t id = simu(ar.id).value();

						while (slot_next != slot_current)
							co_await OnClk(clock);

						simu(ready(*axi->w.d)) = '1';

						BitWidth wordSize = axiCfg.dataW + axiCfg.wUserW;
						for (size_t i = 0; i < len; ++i)
						{
							co_await performTransferWait(*axi->w.d, clock);

							sim::DefaultBitVectorState data = simu((*axi->w.d)->data);
							sim::DefaultBitVectorState user = simu((*axi->w.d)->user);
							sim::DefaultBitVectorState strb = simu((*axi->w.d)->strb);
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
						simu(ready(*axi->w.d)) = '0';
						slot_current++;

						while (slot_next != slot_current_ack)
							co_await OnClk(clock);

						simu(axi->w.b->id) = id;
						simu(axi->w.b->resp) = (size_t)AxiResponseCode::OKAY;
						co_await performTransfer(axi->w.b, clock);
						simu(axi->w.b->id).invalidate();
						simu(axi->w.b->resp).invalidate();
						slot_current_ack++;
					});

					slot_next++;
					while (slot_next - slot_current > 8)
					{
						simu(ready(*axi->w.a)) = '0';
						co_await OnClk(clock);
						simu(ready(*axi->w.a)) = '1';
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

		ready(*out.r.a).exportOverride(ready(*axi.r.a));
		ready(*out.w.a).exportOverride(ready(*axi.w.a));
		ready(*out.w.d).exportOverride(ready(*axi.w.d));

		valid(out.r.d).exportOverride(valid(axi.r.d));
		eop(out.r.d).exportOverride(eop(axi.r.d));
		out.r.d->id.exportOverride(axi.r.d->id);
		out.r.d->data.exportOverride(axi.r.d->data);
		out.r.d->resp.exportOverride(axi.r.d->resp);
		out.r.d->user.exportOverride(axi.r.d->user);

		valid(out.w.b).exportOverride(valid(axi.w.b));
		out.w.b->id.exportOverride(axi.w.b->id);
		out.w.b->resp.exportOverride(axi.w.b->resp);

		return out;
	}
}
