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
	template<TileLinkSignal TLink>
	class TileLinkDemux
	{
		struct Sink
		{
			BOOST_HANA_DEFINE_STRUCT(Sink,
				(TLink, bus),
				(uint64_t, address),
				(size_t, addressBits)
			);
		};
	public:
		virtual void attachSource(TLink& source);
		virtual void attachSink(TLink& sink, uint64_t addressBase);
		void attachSink(TLink&& sink, uint64_t addressBase) { attachSink(sink, addressBase); }

		const TLink& source() const;

		template<typename TArbiterPolicy = ArbiterPolicyLowest>
		void generate();
	private:
		Area m_area = Area("scl_TileLinkDemux");
		bool m_sourceAttached = false;
		bool m_generated = false;
		TLink m_source;
		std::list<Sink> m_sink;
		std::shared_ptr<AddressSpaceDescription> m_addrSpaceDescription = std::make_shared<AddressSpaceDescription>();
	};
}

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkDemux<TLink>::attachSource(TLink& source)
	{
		auto scope = m_area.enter();
		m_source = constructFrom(source);
		m_source <<= source;
		m_sourceAttached = true;

		m_addrSpaceDescription->name = "TileLinkDemux";
		connectAddrDesc(source.addrSpaceDesc, m_addrSpaceDescription);
		connectAddrDesc(m_source.addrSpaceDesc, m_addrSpaceDescription);
	}

	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkDemux<TLink>::attachSink(TLink& sink, uint64_t addressBase)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		HCL_DESIGNCHECK_HINT(m_sourceAttached, "attach source first");
		HCL_DESIGNCHECK_HINT(sink.a->source.width() >= m_source.a->source.width(), "source width too small");

		for (const Sink& s : m_sink)
			HCL_DESIGNCHECK_HINT(s.address != addressBase || s.addressBits != sink.a->address.width().bits(), "address conflict");

		Sink& s = m_sink.emplace_back();
		s.bus = constructFrom(sink);
		sink <<= s.bus;
		s.address = addressBase;
		s.addressBits = sink.a->address.width().bits();

		m_addrSpaceDescription->children.emplace_back(addressBase * 8, sink.addrSpaceDesc);
	}

	template<TileLinkSignal TLink>
	inline const TLink& gtry::scl::TileLinkDemux<TLink>::source() const
	{
		HCL_DESIGNCHECK_HINT(m_sourceAttached, "attach source first");
		return m_source;
	}

	template<TileLinkSignal TLink>
	template<typename TArbiterPolicy>
	inline void TileLinkDemux<TLink>::generate()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_sourceAttached, "attach source first");
		HCL_DESIGNCHECK(!m_generated);
		m_generated = true;

		m_sink.sort([](Sink& a, Sink& b) {
			return a.addressBits < b.addressBits;
		});
		HCL_NAMED(m_source);
		HCL_NAMED(m_sink);

		const UInt& addr = m_source.a->address;

		// connect channel A
		ready(m_source.a) = '0';
		Bit handled = '0';
		for (Sink& s : m_sink)
		{
			const TileLinkA& master = *m_source.a;
			TileLinkA& slave = *s.bus.a;
			slave.opcode = master.opcode;
			slave.param = master.param;
			slave.size = master.size;
			slave.source = zext(master.source);
			slave.address = master.address.lower(slave.address.width());
			slave.mask = master.mask;
			slave.data = master.data;

			valid(s.bus.a) = '0';

			Bit match = addr.upper(addr.width() - s.addressBits) == s.address >> s.addressBits;
			IF(match & !handled)
			{
				// TODO: change handled to check possible conflicts only and not all slaves
				handled = '1';
				valid(s.bus.a) = valid(m_source.a);
				upstream(m_source.a) = upstream(s.bus.a);
			}
		}
		HCL_NAMED(handled);

		// handle access to unmapped areas
		// TODO: check if there are any unmapped areas first
		TLink unmapped = constructFrom(m_source);
		downstream(unmapped.a) = downstream(m_source.a);
		IF(!handled)
			upstream(m_source.a) = upstream(unmapped.a);
		ELSE
			valid(unmapped.a) = '0';

		tileLinkErrorResponder(unmapped);
		HCL_NAMED(unmapped);

		// connect channel D
		uint32_t sortKey = -1;
		StreamArbiter<TileLinkChannelD, TArbiterPolicy> arbiter;
		arbiter.attach(strm::regDownstream(move(*unmapped.d)), sortKey--);

		for (Sink& s : m_sink)
			arbiter.attach(*s.bus.d, sortKey--);
		*m_source.d <<= arbiter.out();
		arbiter.generate();
	}
}
