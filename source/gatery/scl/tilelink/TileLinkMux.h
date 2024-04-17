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
#include "../stream/StreamArbiter.h"

namespace gtry::scl
{
	template<TileLinkSignal TLink = TileLinkUL>
	class TileLinkMux
	{
	public:
		TileLinkMux();

		virtual TileLinkMux& attachSource(TLink& source);

	//	template<typename TArbiterPolicy = ArbiterPolicyLowest>
		TLink generate();
	protected:
		void generateChanA(TileLinkChannelA& a);
		void generateChanD(TileLinkChannelD& d);
		void addSourceIndex();

	private:
		Area m_area = Area{ "scl_TileLinkMux", true };
		bool m_generated = false;
		std::list<TLink> m_source;
		
		std::shared_ptr<AddressSpaceDescription> m_addrSpaceDescription = std::make_shared<AddressSpaceDescription>();
	};
}

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	inline TileLinkMux<TLink>::TileLinkMux()
	{
		m_area.leave();
	}

	template<TileLinkSignal TLink>
	inline TileLinkMux<TLink>& TileLinkMux<TLink>::attachSource(TLink& source)
	{
		HCL_DESIGNCHECK_HINT(!m_generated, "already generated");
		for (const TLink& l : m_source)
		{
			HCL_DESIGNCHECK_HINT(l.a->address.width() == source.a->address.width(), "address width of all sources must match");
			HCL_DESIGNCHECK_HINT(l.a->data.width() == source.a->data.width(), "data width of all sources must match");
			HCL_DESIGNCHECK_HINT((*l.d)->sink.width() == (*source.d)->sink.width(), "sink width of all sources must match");
			break;
		}

		auto ent = m_area.enter();
		TLink& link = m_source.emplace_back();
		link = constructFrom(source);
		link <<= source;
		connectAddrDesc(source.addrSpaceDesc, m_addrSpaceDescription);

		return *this;
	}

	template<TileLinkSignal TLink>
//	template<typename TArbiterPolicy>
	inline TLink gtry::scl::TileLinkMux<TLink>::generate()
	{
		HCL_DESIGNCHECK_HINT(!m_source.empty(), "attach all source links first");
		HCL_DESIGNCHECK(!m_generated);
		m_generated = true;

		auto ent = m_area.enter();
		HCL_NAMED(m_source);
		
		TLink sink;
		generateChanA(sink.a);
		generateChanD(*sink.d);

		connectAddrDesc(m_addrSpaceDescription, sink.addrSpaceDesc);
		HCL_NAMED(sink);
		return sink;
	}

	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkMux<TLink>::generateChanD(TileLinkChannelD& d)
	{
		d = constructFrom(*m_source.front().d);
		d->source.resetNode();
		d->source = m_source.front().a->source.width();
		
		size_t idx = 0;
		BitWidth myTagW = BitWidth::count(m_source.size());
		ready(d) = '0';

		if (!m_source.empty())
			ready(d) = ready(*m_source.front().d);

		for (TLink& src : m_source)
		{
			(*src.d)->opcode = d->opcode;
			(*src.d)->param = d->param;
			(*src.d)->size = d->size;
			(*src.d)->source = d->source.lower((*src.d)->source.width());
			(*src.d)->sink = d->sink;
			(*src.d)->data = d->data;
			(*src.d)->error = d->error;
			
			valid(*src.d) = valid(d) & d->source.upper(myTagW) == idx;
			IF(valid(*src.d))
				ready(d) = ready(*src.d);
			idx++;
		}
	}

	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkMux<TLink>::generateChanA(TileLinkChannelA& a)
	{
		addSourceIndex();
		StreamArbiter<TileLinkChannelA> arbiter;
		for (TLink& src : m_source)
			arbiter.attach(src.a);
		arbiter.generate();

		a = constructFrom(arbiter.out());
		a <<= arbiter.out();
	}

	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkMux<TLink>::addSourceIndex()
	{
		BitWidth myTagW = BitWidth::count(m_source.size());
		BitWidth srcTagW = 0_b;
		for (const TLink& src : m_source)
			srcTagW = std::max(srcTagW, src.a->source.width());

		size_t idx = 0;
		for (TLink& src : m_source)
		{
			UInt tag = src.a->source;
			src.a->source.resetNode();
			src.a->source = cat(ConstUInt(idx, myTagW), zext(tag, srcTagW));
			idx++;
		}
	}
}
