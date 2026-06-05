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
#include "TileLinkCache.h"

namespace gtry::scl
{
	TileLinkU tileLinkCacheDirectMappedReadOnlyBurst(TileLinkU&& ram, gtry::BitWidth addrW, std::function<void(TileLinkChannelA&)> selectBurst, const sim::DefaultBitVectorState& initialContent)
	{
		Area ent{ "scl_RoCache", true };
		scl::TileLinkU cpu = constructFrom(ram);
		HCL_NAMED(ram);

		const size_t bytePerBeatLog = utils::Log2(cpu.a->data.width().bytes());
		const BitWidth tagW = cpu.a->address.width() - bytePerBeatLog - addrW;
		Memory<internal::RoCacheData> cacheMem(addrW.count(), internal::RoCacheData{
			.tag = tagW,
			.value = cpu.a->data.width()
			});
		HCL_NAMED(cacheMem);
		cacheMem.setType(MemType::DONT_CARE, 1);

		if (initialContent.size() == 0)
		{
			cacheMem.initZero();
		}
		else
		{
			size_t elementBits = width(cacheMem.defaultValue()).bits();
			size_t wordBits = cpu.a->data.width().bits();

			sim::DefaultBitVectorState initContent;
			initContent.resize(addrW.count() * elementBits);
			for (size_t i = 0; i < addrW.count(); ++i)
			{
				// TAG Value
				initContent.setRange(sim::DefaultConfig::DEFINED, i * elementBits, tagW.bits());
				initContent.clearRange(sim::DefaultConfig::VALUE, i * elementBits, tagW.bits());

				// valid bit
				initContent.set(sim::DefaultConfig::DEFINED, i * elementBits + elementBits - 1);

				if(i * wordBits < initialContent.size())
				{
					initContent.set(sim::DefaultConfig::VALUE, i * elementBits + elementBits - 1);
					initContent.copyRange(i * elementBits + tagW.bits(), initialContent, i * wordBits, wordBits);
				}
				else
				{
					initContent.clear(sim::DefaultConfig::VALUE, i * elementBits + elementBits - 1);
				}
			}
			cacheMem.fillPowerOnState(move(initContent));
		}

		// buffer request from cpu
		scl::TileLinkChannelA req = regDownstream(move(cpu.a)) | name("req");

		// the line lookup address is forwarded to skip one wait cycle
		UInt lineAddr = cpu.a->address(bytePerBeatLog, addrW);
		IF(valid(req) & !ready(req))
			lineAddr = req->address(bytePerBeatLog, addrW);
		HCL_NAMED(lineAddr);

		// count response beats for cache address offset
		scl::Counter burstOffset{ BitWidth{ ram.cfg.burstLimitLog - bytePerBeatLog } };

		// write ram response to cache memory
		UInt ramAddr = ram.a->address.width(); ramAddr = reg(ramAddr) | name("ramAddr");
		IF(transfer(*ram.d))
		{
			UInt writeAddr = ramAddr(bytePerBeatLog, addrW) ^ zext(burstOffset.value()) | name("writeAddr");
			cacheMem[writeAddr] = internal::RoCacheData{
				.tag = ramAddr.upper(tagW),
				.value = (*ram.d)->data,
				.valid = '1',
			};
			burstOffset.inc();
		}

		// read cache memory and check for hit/miss
		internal::RoCacheData line = reg(cacheMem[lineAddr], { .allowRetimingBackward = true }) | name("line");
		Bit cacheHit = (valid(req) & line.valid & line.tag == req->address.upper(tagW)) | name("cacheHit");

		// respond to cpu on cache hit. or wait until cache hit.
		**cpu.d = tileLinkDefaultResponse(*req);
		(*cpu.d)->data = line.value;
		valid(*cpu.d) = cacheHit;
		ready(req) = transfer(*cpu.d);

		// keep track of outstanding ram requests
		Bit ramBusy = scl::flag(transfer(ram.a), transfer(*ram.d) & eop(*ram.d)) | name("ramBusy");
		//sim_assert(!valid(*ram.d) | eop(*ram.d)) << "ram response must be last beat";

		// issue ram request on cache miss
		*ram.a = *req;
		IF(ram.a->address.lower(3_b) == 0)
			ram.a->size = 3;
		if(selectBurst)
			selectBurst(ram.a);

		// request burst only from dram
		valid(ram.a) = !ramBusy & valid(req) & !cacheHit;
		ready(*ram.d) = '1';

		IF(!ramBusy)
		{
			burstOffset.reset();
			ramAddr = req->address;
		}

		HCL_NAMED(cpu);
		return cpu;
	}

	TileLinkU tileLinkCacheDirectMappedVictim(TileLinkU&& ram, BitWidth addrW)
	{
		Area ent{ "scl_VictimCache", true };
		HCL_NAMED(ram);

		auto cpu = tileLinkInit<TileLinkU>(
			ram.a->address.width(), 
			ram.a->data.width(), 
			ram.a->source.width(), 
			ram.a->size.width()) | name("cpu");

		// create cache memory
		const size_t bytePerBeatLog = utils::Log2(cpu.a->data.width().bytes());
		const BitWidth tagW = cpu.a->address.width() - bytePerBeatLog - addrW;
		Memory<internal::RoCacheData> cacheMem(addrW.count(), internal::RoCacheData{
			.tag = tagW,
			.value = cpu.a->data.width()
			});
		HCL_NAMED(cacheMem);
		cacheMem.setType(MemType::DONT_CARE, 1);
		cacheMem.initZero();

		// read/write cache line based on cpu request
		UInt lineAddr = cpu.a->address(bytePerBeatLog, addrW) | name("lineAddr");
		auto cachePort = cacheMem[lineAddr];
		
		IF(valid(cpu.a) & cpu.a->isPut())
		{
			cachePort = internal::RoCacheData{
				.tag = cpu.a->address.upper(tagW),
				.value = cpu.a->data,
				.valid = '1',
			};
		}

		internal::RoCacheData line;
		ENIF(ready(cpu.a))
			line = reg(cachePort.read(), { .allowRetimingBackward = true }) | name("line");
		
		// check match
		TileLinkChannelA req = regDownstream(move(cpu.a)) | name("req");
		Bit hit = line.valid & line.tag == req->address.upper(tagW) | name("hit");

		ram.a <<= req;		// forward request to ram interface
		*cpu.d <<= *ram.d;	// forward ram response to cpu interface
		IF(valid(req) & hit & req->isGet()) 
		{
			// mask cache hits on ram interface
			valid(ram.a) = '0';

			// generate get response
			**cpu.d = tileLinkDefaultResponse(*req);
			(*cpu.d)->data = line.value;
			valid(*cpu.d) = valid(req);
			ready(req) = ready(*cpu.d);

			// hold back ram responses
			ready(*ram.d) = '0';
		}
		
		return cpu;
	}

	TileLinkU TileLinkDataCache::generate(TileLinkU&& memory)
	{
		Area ent{ "scl_DataCache", true };
		HCL_NAMED(memory);

		TileLinkCfg cpuCfg = memory.cfg;
		cpuCfg.burstLimitLog = cpuCfg.burstThresholdLog();
		TileLinkU cpu = tileLinkInit(cpuCfg);

		SetData blueprint;
		blueprint.way.resize(m_cfg.wayAddrW.count());
		for (Way& way : blueprint.way) way.tagAddr = m_cfg.tagAddrW(memory.a->wordAddress().width());

		Memory<SetData> tagMem(m_cfg.setAddrW, blueprint);
		tagMem.setType(MemType::DONT_CARE, 1);
		tagMem.noConflicts();
		tagMem.initZero();
		tagMem.setName("tagMem");

		StreamBroadcaster cpuReq = move(cpu.a) |
			strm::transform<strm::InsertPos::attach>([&](const TileLinkA& a) { return tagMem[setAddr(a.wordAddress())].read(); }) |
			strm::transform<strm::InsertPos::attach>(std::bind_front(&TileLinkDataCache::cacheInfo, this)) |
			strm::regDecoupleGap({ .allowRetimingBackward = true }) |
			name("cpuReq");

		Bit handledByPolicy;
		Stream cacheDataReq = cpuReq(handledByPolicy) | name("cacheDataReq");

		// the policy is used to handle cases where cache is used or needs to be updated
		handledByPolicy = get<CacheInfo>(cacheDataReq).hit | get<CacheInfo>(cacheDataReq).cacheable & cacheDataReq->isGet();
		handledByPolicy = name(handledByPolicy, "handledByPolicy");

		Bit enableMemoryRequest;
		memory.a <<= reduceTo<TileLinkChannelA>(cpuReq(enableMemoryRequest));

		// the only case where we don't isse a memory request is a read hit
		enableMemoryRequest = !(cacheDataReq->isGet() & get<CacheInfo>(cacheDataReq).hit) | name("enableMemoryRequest");

		// in case its a cacheable read miss we expand the request to the entire cache line
		IF(cacheDataReq->isGet() & !get<CacheInfo>(cacheDataReq).hit & get<CacheInfo>(cacheDataReq).cacheable | name("issueBurst"))
		{
			BitWidth logBurstW = m_cfg.lineWordAddrW + BitWidth::count(memory.cfg.dataW.bytes());
			memory.a->address.lower(logBurstW) = 0;
			memory.a->size = logBurstW.bits();
		}

		const CacheInfo& info = get<CacheInfo>(cacheDataReq);
		UInt dataReadAddr = dataAddr(cacheDataReq->wordAddress(), info.way);
		Memory<BVec> dataMem(dataReadAddr.width(), memory.cfg.dataW);
		dataMem.noConflicts();
		dataMem.setType(MemType::DONT_CARE, 1);
		dataMem.setName("dataMem");

		TileLinkD cacheDataResD = tileLinkDefaultResponse(*cacheDataReq);
		cacheDataResD.opcode = (size_t)TileLinkD::AccessAckData; // we only respond in case of read hit
		cacheDataResD.data = dataMem[dataReadAddr];
		TileLinkChannelD cacheDataRes = strm::createVStream(cacheDataResD, '0') | strm::attach(Ready{});

		Bit wr;
		UInt wrAddr = dataMem.addressWidth();
		BVec wrData = memory.cfg.dataW;
		BVec wrMask = memory.a->mask.width();
		IF(wr)
			dataMem[wrAddr].write(wrData, wrMask);
		wr = '0';
		wrAddr = dataReadAddr;
		wrData = cacheDataReq->data;
		wrMask = cacheDataReq->mask;

		TileLinkChannelD memRes = constructFrom(*memory.d);
		memRes <<= *memory.d; // create a copy so we can safely filter and modify on its way to the cpu
		Bit memEop;
		UInt memOfsCtr;
		ENIF(transfer(memRes) & memRes->hasData())
			std::tie(memEop, memOfsCtr) = eop(memRes->size, memRes->data.width(), '0');
		UInt memOfs = memOfsCtr.lower(m_cfg.lineWordAddrW) | name("memOfs");

		// from here on the cache policy is implemented
		ready(cacheDataReq) = '0';
		IF(valid(cacheDataReq))
		{
			IF(info.hit & cacheDataReq->isGet())
			{
				// cache read hit, wait for a bubble in memory response channel to insert our result
				IF(!valid(memRes))
				{
					valid(cacheDataRes) = '1'; // read from cache data memory
					ready(cacheDataReq) = '1'; // ack request
				}
			}

			IF(info.hit & !cacheDataReq->isGet())
			{
				// cache write hit, update data memory
				wr = '1';
				ready(cacheDataReq) = '1';
			}

			IF(!info.hit & cacheDataReq->isGet())
			{
				// cache read miss. wait for memory response and release cpu

				// update tag memory
				// TODO: use masked write instead of forwarding all the set data from tag lookup
				SetData setData = get<SetData>(cacheDataReq);
				Way way{ .valid = '1', .tagAddr = tagAddr(cacheDataReq->wordAddress()) };
				demux(info.way, setData.way, way);
				tagMem[setAddr(cacheDataReq->wordAddress())] = setData;

				// update data memory from memory response
				wr = valid(memRes) & memRes->hasData();
				wrAddr.lower(memOfs.width()) = memOfs;
				wrData = (memRes)->data;
				wrMask = wrMask.width().mask();

				// read request finished
				IF(valid(memRes) & memRes->hasData() & memEop)
					ready(cacheDataReq) = '1';

				// hide memory response from cpu except for requested word
				valid(memRes) &= (!memRes->hasData() | cacheDataReq->wordAddress().lower(memOfs.width()) == memOfs) | name("requestedWordRes");

				// memory size is always burst, cpu expects its requested size
				(memRes)->size = cacheDataReq->size;
			}
		}

		*cpu.d <<= move(memRes) |
			name("memRes") |
			strm::regDownstreamBlocking() |
			name("cpuRes");

		TileLinkChannelD cacheDataResReg = move(cacheDataRes) |
			name("cacheDataRes") |
			strm::regDownstreamBlocking({ .allowRetimingBackward = true }) |
			name("cacheDataResReg");

		ready(cacheDataResReg) = ready(*cpu.d);
		IF(valid(cacheDataResReg))
		{
			sim_assert(!valid(*cpu.d)) << "cache hit scheduled over memory response";
			valid(*cpu.d) = '1';
			**cpu.d = *cacheDataResReg;
		}

		HCL_NAMED(cpu);
		return cpu;
	}

	UInt TileLinkDataCache::setAddr(const UInt& wordAddr) const 
	{ 
		return wordAddr(m_cfg.lineWordAddrW.bits(), m_cfg.setAddrW); 
	}
	
	UInt TileLinkDataCache::tagAddr(const UInt& wordAddr) const 
	{ 
		return wordAddr.upper(m_cfg.tagAddrW(wordAddr.width())); 
	}

	UInt TileLinkDataCache::dataAddr(const UInt& wordAddr, const UInt& way) const 
	{ 
		return cat(setAddr(wordAddr), way, wordAddr.lower(m_cfg.lineWordAddrW)); 
	}

	TileLinkDataCache::CacheInfo TileLinkDataCache::cacheInfo(const TileLinkA& a, const SetData& set)
	{
		VStream<UInt> evition =
			strm::lfsr(BitWidth::count(set.way.size()), 0x12000, UInt{ "20d1337" }) |
			strm::removeReady() |
			name("eviction");

		CacheInfo out{
			.hit = '0',
			.cacheable = !a.address.msb(),
			.way = *evition,
		};

		for (size_t i = 0; i < set.way.size(); ++i)
		{
			IF(!set.way[i].valid)
				out.way = i;
		}

		for (size_t i = 0; i < set.way.size(); ++i)
		{
			IF(set.way[i].valid & set.way[i].tagAddr == tagAddr(a.wordAddress()) | name("cache_hit" + std::to_string(i)))
			{
				out.hit = '1';
				out.way = i;
			}
		}
		return out;
	}

}
