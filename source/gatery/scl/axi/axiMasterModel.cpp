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
#include "axiMasterModel.h"
#include <gatery/scl/stream/SimuHelpers.h>

namespace gtry::scl
{
	void simInit(Axi4& axi)
	{
		simu(valid(*axi.r.a)) = '0';
		simu(valid(*axi.w.a)) = '0';
		simu(valid(*axi.w.d)) = '0';
		simu(ready(axi.w.b)) = '1';
		simu(ready(axi.r.d)) = '1';
	}

	SimFunction<std::tuple<uint64_t, uint64_t, bool>> simGet(Axi4& axi, uint64_t address, uint64_t logByteSize, const Clock& clk)
	{
		size_t beatCount = (1ull << logByteSize) / axi.r.d->data.width().bytes();

		fork([&]()->SimProcess {
			AxiAddress& req = **axi.r.a;
			simu(req.id) = 0;
			simu(req.addr) = address;
			simu(req.len) = beatCount - 1;
			simu(req.size)= utils::Log2C(axi.r.d->data.width().bytes());
			simu(req.burst) = (size_t)AxiBurstType::INCR;
			simu(req.cache) = 0;
			simu(req.prot) = 0;
			simu(req.qos) = 0;
			simu(req.region) = 0;
			simu(req.user) = 0;

			co_await performTransfer(*axi.r.a, clk);

			simu(req.id).invalidate();
			simu(req.addr).invalidate();
			simu(req.len).invalidate();
			simu(req.size).invalidate();
			simu(req.burst).invalidate();
			simu(req.cache).invalidate();
			simu(req.prot).invalidate();
			simu(req.qos).invalidate();
			simu(req.region).invalidate();
			simu(req.user).invalidate();
		});

		uint64_t data = 0;
		uint64_t defined = 0;
		bool error = false;

		for (size_t i = 0; i < beatCount; ++i)
		{
			co_await performTransferWait(axi.r.d, clk);

			auto respData = simu(axi.r.d->data);
			data |= respData.value() << i * axi.r.d->data.width().bits();
			defined |= respData.defined() << i * axi.r.d->data.width().bits();
			error |= simu(axi.r.d->resp) > 1;
		}
		error |= simu(eop(axi.r.d)) == '0';

		co_return std::make_tuple(data, defined, error);
	}

	SimFunction<bool> simPut(Axi4& axi, uint64_t address, uint64_t logByteSize, uint64_t data, const Clock& clk)
	{
		HCL_ASSERT_HINT(1ull << logByteSize >= axi.r.d->data.width().bytes(), "non full word and burst transfers not implemented");
		size_t beatCount = (1ull << logByteSize) / axi.r.d->data.width().bytes();

		fork([&]()->SimProcess {

			AxiAddress& req = **axi.w.a;
			simu(req.id) = 0;
			simu(req.addr) = address;
			simu(req.len) = beatCount - 1;
			simu(req.size) = utils::Log2C(axi.r.d->data.width().bytes());
			simu(req.burst) = (size_t)AxiBurstType::INCR;
			simu(req.cache) = 0;
			simu(req.prot) = 0;
			simu(req.qos) = 0;
			simu(req.region) = 0;
			simu(req.user) = 0;

			co_await performTransfer(*axi.w.a, clk);

			simu(req.id).invalidate();
			simu(req.addr).invalidate();
			simu(req.len).invalidate();
			simu(req.size).invalidate();
			simu(req.burst).invalidate();
			simu(req.cache).invalidate();
			simu(req.prot).invalidate();
			simu(req.qos).invalidate();
			simu(req.region).invalidate();
			simu(req.user).invalidate();
		});

		fork([&]()->SimProcess {
			AxiWriteData& req = **axi.w.d;

			for (size_t i = 0; i < beatCount; ++i)
			{
				simu(req.data) = data & req.data.width().mask();
				simu(req.strb) = req.strb.width().mask();
				simu(req.user) = 0;
				simu(eop(*axi.w.d)) = i + 1 == beatCount;

				co_await performTransfer(*axi.w.d, clk);
				data >>= req.data.width().bits();
			}

			simu(req.data).invalidate();
			simu(req.strb).invalidate();
			simu(req.user).invalidate();
			simu(eop(*axi.w.d)).invalidate();
		});

		co_await performTransferWait(axi.w.b, clk);
		co_return simu(axi.w.b->resp) > 1;
	}
}
