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
#include "../stream/StreamDemux.h"

namespace gtry::scl
{
	class TileLinkAddressDecoder
	{
		struct Slave
		{
			TileLinkU link;
			uint64_t addressOffset;
			uint32_t responsePriority;
		};

	public:

		TileLinkAddressDecoder& operator () (uint64_t addressOffset, TileLinkU&& slave);
		TileLinkAddressDecoder& respondToUnmapped(bool val) { m_useErrorResponder = val; return *this; }
		TileLinkU generate(std::string instanceName = {});

	protected:
		std::list<Slave> m_slave;
		bool m_useErrorResponder = true;
	};

	class TileLinkArbiter
	{
	public:
		TileLinkArbiter(TileLinkU&& slave, BitWidth arbiterSourceW);

		operator TileLinkU ();
		TileLinkU out() { return (TileLinkU)*this; }

		void generate();

	protected:

	private:
		Area m_ent = Area{ "scl_TileLinkArbiter", true };

		BitWidth m_arbiterSourceW;
		size_t m_portIndex = 0;
		TileLinkCfg m_cfg;
		AddressSpaceDescriptionHandle m_addressSpace;

		StreamArbiter<TileLinkChannelA> m_handlerA;
		StreamRouter<TileLinkChannelD> m_handlerD;
	};

	TileLinkU tileLinkErrorResponder(const TileLinkCfg& cfg);
}
