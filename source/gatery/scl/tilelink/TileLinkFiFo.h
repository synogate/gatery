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
#include "../stream/streamFifo.h"

namespace gtry::scl
{
	class TileLinkFifo
	{
	public:
		TileLinkFifo();

		template<TileLinkSignal TLink> TileLinkFifo& connectSlave(TLink& link, size_t depthMin = 0);
		template<TileLinkSignal TLink> TLink connectMaster();

	private:
		Area m_area = { "scl_TileLinkFifo", true };
		Fifo<TileLinkA> m_a;
		Fifo<TileLinkD> m_d;

		bool m_slaveConnected = false;
		bool m_masterConnected = false;
		
		std::shared_ptr<AddressSpaceDescription> m_addrSpaceDescription = std::make_shared<AddressSpaceDescription>();
	};

	template<TileLinkSignal TLink>
	TLink tileLinkFifo(TLink& link, size_t depthMin = 0);
}

namespace gtry::scl
{
	inline TileLinkFifo::TileLinkFifo()
	{
		m_area.leave();
	}

	template<TileLinkSignal TLink>
	TileLinkFifo& TileLinkFifo::connectSlave(TLink& link, size_t depthMin)
	{
		HCL_DESIGNCHECK(!m_slaveConnected);
		m_slaveConnected = true;
		auto ent = m_area.enter();

		if(depthMin == 0)
			depthMin = txid(link.a).width().count();

		m_a.setup(depthMin, *link.a);
		link.a <<= strm::pop(m_a);

		if (m_d.depth() >= txid(link.a).width().count())
			ready(*link.d) = '1';

		m_d.setup(depthMin, **link.d);
		push(m_d, move(*link.d));

		connectAddrDesc(m_addrSpaceDescription, link.addrSpaceDesc);

		return *this;
	}

	template<TileLinkSignal TLink>
	TLink TileLinkFifo::connectMaster()
	{
		HCL_DESIGNCHECK(m_slaveConnected);
		HCL_DESIGNCHECK(!m_masterConnected);
		m_masterConnected = true;
		auto ent = m_area.enter();

		TLink ret{
			.a = constructFrom(m_a.peek()),
			.d = constructFrom(m_d.peek()),
			.addrSpaceDesc = m_addrSpaceDescription
		};

		if (m_a.depth() >= txid(ret.a).width().count())
			ready(ret.a) = '1';

		push(m_a, move(ret.a));
		m_a.generate();

		*ret.d <<= strm::pop(m_d);
		m_d.generate();

		return ret;
	}

	template<TileLinkSignal TLink>
	TLink tileLinkFifo(TLink& link, size_t depthMin)
	{
		return TileLinkFifo{}.connectSlave(link, depthMin).template connectMaster<TLink>();
	}
}
