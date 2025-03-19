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
	TileLinkUL tileLinkCacheDirectMappedReadOnlyBurst(TileLinkUL&& ram, gtry::BitWidth addrW, std::function<void(TileLinkChannelA&)> selectBurst)
	{
		Area ent{ "scl_RoCache", true };
		scl::TileLinkUL cpu = constructFrom(ram);
		HCL_NAMED(ram);

		const size_t bytePerBeatLog = utils::Log2(cpu.a->data.width().bytes());
		const BitWidth tagW = cpu.a->address.width() - bytePerBeatLog - addrW;
		Memory<internal::RoCacheData> cacheMem(addrW.count(), internal::RoCacheData{
			.tag = tagW,
			.value = cpu.a->data.width()
			});
		HCL_NAMED(cacheMem);
		cacheMem.setType(MemType::DONT_CARE, 1);
		cacheMem.initZero();

		// buffer request from cpu
		scl::TileLinkChannelA req = regDownstream(move(cpu.a)) | name("req");

		// the line lookup address is forwarded to skip one wait cycle
		UInt lineAddr = cpu.a->address(bytePerBeatLog, addrW);
		IF(valid(req) & !ready(req))
			lineAddr = req->address(bytePerBeatLog, addrW);
		HCL_NAMED(lineAddr);

		// count response beats for cache address offset
		scl::Counter burstOffset{ 4 };

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

	TileLinkUL tileLinkCacheDirectMappedVictim(TileLinkUL&& ram, BitWidth addrW)
	{
		Area ent{ "scl_VictimCache", true };
		HCL_NAMED(ram);

		auto cpu = tileLinkInit<TileLinkUL>(
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
}
