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
#include "../Fifo.h"

namespace gtry::scl
{
	class TileLinkFifo
	{
	public:
		template<TileLinkSignal TLink> TileLinkFifo& connectSlave(TLink& link, size_t depthMin = 0);
		template<TileLinkSignal TLink> TLink connectMaster();

	private:
		Fifo<TileLinkA> m_a;
		Fifo<TileLinkD> m_d;

		bool m_slaveConnected = false;
		bool m_masterConnected = false;
	};

	template<TileLinkSignal TLink>
	TLink tileLinkFifo(TLink& link, size_t depthMin = 0);
}

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	TileLinkFifo& TileLinkFifo::connectSlave(TLink& link, size_t depthMin)
	{
		HCL_DESIGNCHECK(!m_slaveConnected);
		m_slaveConnected = true;

		if(depthMin == 0)
			depthMin = txid(link.a).width().count();

		m_a.setup(depthMin, *link.a);
		link.a <<= m_a;

		m_d.setup(depthMin, **link.d);
		m_d <<= *link.d;

		return *this;
	}

	template<TileLinkSignal TLink>
	TLink TileLinkFifo::connectMaster()
	{
		HCL_DESIGNCHECK(m_slaveConnected);
		HCL_DESIGNCHECK(!m_masterConnected);
		m_masterConnected = true;

		TLink ret{
			.a = constructFrom(m_a.peek()),
			.d = constructFrom(m_d.peek())
		};

		m_a <<= ret.a;
		m_a.generate();

		*ret.d <<= m_d;
		m_d.generate();

		return ret;
	}

	template<TileLinkSignal TLink>
	TLink tileLinkFifo(TLink& link, size_t depthMin)
	{
		return TileLinkFifo{}.connectSlave(link, depthMin).connectMaster<TLink>();
	}
}
