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
#include <span>

#include <gatery/frontend.h>

#include "../stream/Stream.h"

#include <gatery/frontend/Reverse.h>

namespace gtry::scl
{
	struct OneHot : UInt
	{
		OneHot() : UInt() {}
		OneHot(const OneHot&) = default;

		OneHot(BitWidth width) : UInt(width, Expansion::none) {}
		explicit OneHot(const UInt& initValue) : UInt(initValue) {}

		OneHot& operator = (const OneHot&) = default;

		void setBit(const UInt& idx);
	};

	OneHot decoder(const UInt& in);
	UInt encoder(const OneHot& in);

	std::vector<VStream<UInt>> makeIndexList(const UInt& valids);

	VStream<UInt> priorityEncoder(const UInt& in);
	VStream<UInt> priorityEncoderTree(const UInt& in, bool registerStep, size_t resultBitsPerStep = 2);
}
