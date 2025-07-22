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
#include "TileLinkErrorResponder.h"
#include "../stream/StreamArbiter.h"

namespace gtry::scl
{
	scl::TileLinkU tileLinkHalfWidth(scl::TileLinkU&& slave);
	inline auto tileLinkHalfWidth() { return [=](auto&& in) { return tileLinkHalfWidth(std::forward<decltype(in)>(in)); }; }

	scl::TileLinkU tileLinkDoubleWidth(scl::TileLinkU& slave);
	inline scl::TileLinkU tileLinkDoubleWidth(scl::TileLinkU&& slave) { return tileLinkDoubleWidth(slave); }
	inline auto tileLinkDoubleWidth() {	return [=](auto&& in) { return tileLinkDoubleWidth(std::forward<decltype(in)>(in)); }; }

	scl::TileLinkU tileLinkAddBurst(scl::TileLinkU&& slave, BitWidth sizeW);
	inline auto tileLinkAddBurst(BitWidth sizeW) { return [=](auto&& in) { return tileLinkAddBurst(std::forward<decltype(in)>(in), sizeW); }; }
}
