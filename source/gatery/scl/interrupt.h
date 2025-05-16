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

#include "memoryMap/MemoryMapConnectors.h"

namespace gtry::scl
{
	// holds interrupt line high on request until request became low and ack high for at least one cycle
	// on por requests are ignored until req bacame low for at least one cycle. this is useful for por swings on buttons.
	inline BVec interruptLogic(BVec req, BVec ack)
	{
		Area ent{ "scl_interruptLogic", true };
		HCL_NAMED(req);
		HCL_NAMED(ack);

		HCL_DESIGNCHECK(req.width() == ack.width());
		BVec reqState = req.width();
		BVec ackState = req.width();

		reqState |= req;
		ackState |= ack;

		BVec done = (~req & ackState) | name("done");
		reqState &= ~done;
		ackState &= ~done;

		reqState = reg(reqState, ConstBVec(0, req.width())) | name("reqState");
		ackState = reg(ackState, ConstBVec(req.width().mask(), req.width())) | name("ackState");

		return (reqState & ~ackState) | name("out");
	}

	inline void interruptLogic(BVec req, MemoryMap& mmap, const std::string& name)
	{
		BVec ack = ConstBVec(0, req.width());
		mapIn(mmap, ack, name);
		mapOut(mmap, interruptLogic(req, ack), name);
	}
}
