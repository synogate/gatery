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
	TileLinkUL tileLinkCacheDirectMappedReadOnly(TileLinkUL&& out, BitWidth addrW)
	{
		Area ent{ "scl_RoCache", true };
		//HCL_NAMED(link);

		scl::TileLinkUL link = constructFrom(out);
		*link.d <<= *out.d;

		const size_t bytePerBeatLog = utils::Log2(link.a->data.width().bytes());
		const BitWidth tagW = link.a->address.width() - bytePerBeatLog - addrW;
		Memory<internal::RoCacheData> cacheMem(addrW.count(), internal::RoCacheData{
			.tag = tagW,
			.value = link.a->data.width()
			});
		HCL_NAMED(cacheMem);
		cacheMem.noConflicts();
		cacheMem.setType(MemType::DONT_CARE, 1);
		cacheMem.initZero();

		UInt reqAddr = link.a->address.width();
		IF(valid(out.a))
			reqAddr = out.a->address;
		reqAddr = reg(reqAddr);
		HCL_NAMED(reqAddr);

		IF(valid(*out.d))
		{
			cacheMem[reqAddr(bytePerBeatLog, addrW)] = internal::RoCacheData{
				.tag = reqAddr.upper(tagW),
				.value = (*out.d)->data,
				.valid = '1',
			};
		}

		internal::RoCacheData line = cacheMem[link.a->address(bytePerBeatLog, addrW)];
		line = reg(line, RegisterSettings{ .allowRetimingBackward = true });
		HCL_NAMED(line);

		scl::TileLinkChannelA req = scl::strm::regDownstream(move(link.a));
		HCL_NAMED(req);

		// cache hit
		IF(valid(req) & line.valid & line.tag == req->address.upper(tagW))
		{
			ready(req) = ready(*link.d);
			valid(*link.d) = '1';
			**link.d = scl::tileLinkDefaultResponse(*req);
			(*link.d)->data = line.value;

			valid(req) = '0';
		}

		out.a <<= req;

		HCL_NAMED(link);
		return link;
	}

	TileLinkUL tileLinkCacheDirectMappedReadOnlyBurst(TileLinkUL&& ram, gtry::BitWidth addrW)
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
		// request burst only from dram
		IF(ram.a->address.msb() == '0' & ram.a->address.lower(3_b) == 0)
			ram.a->size = 3;
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
}
