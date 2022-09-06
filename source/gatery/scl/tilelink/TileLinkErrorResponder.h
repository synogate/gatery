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
#pragma once
#include <gatery/frontend.h>
#include "tilelink.h"
#include "../Counter.h"

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	void tileLinkErrorResponder(TLink& link)
	{
		Area ent{ "scl_tileLinkErrorResponder", true };
		HCL_NAMED(link);

		auto& d = *link.d;
		valid(d) = valid(link.a);
		d->opcode = responseOpCode(link);
		d->param = 0;
		d->size = link.a->size;
		d->source = link.a->source;
		d->sink = 0;
		d->data = ConstBVec(link.a->data.width());

		if (!link.template capability<TileLinkCapBurst>())
		{
			d->error = '1';
			ready(link.a) = ready(d);
		}
		else
		{
			UInt len = transferLength(d);
			HCL_NAMED(len);

			scl::Counter beatCounter{ len };

			d->error = '0';
			IF(transfer(d))
				beatCounter.inc();
			
			ready(link.a) = '0';
			IF(beatCounter.isLast())
			{
				d->error = '1';
				ready(link.a) = ready(d);
			}
		}

		HCL_NAMED(link);
	}
}
