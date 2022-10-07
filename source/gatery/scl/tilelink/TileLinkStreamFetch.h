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

namespace gtry::scl
{
	class TileLinkStreamFetch
	{
	public:
		struct Command
		{
			UInt address;
			UInt beats;
		};

	public:
		TileLinkStreamFetch();

		virtual TileLinkUL generate(RvStream<Command>& cmdIn, RvStream<BVec>& dataOut);
	private:
		Area m_area = {"scl_TileLinkStreamFetch", true};


	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::TileLinkStreamFetch::Command, address, beats);
