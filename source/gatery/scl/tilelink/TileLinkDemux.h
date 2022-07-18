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
	template<TileLinkSignal TLink>
	class TileLinkDemux
	{
		struct Sink
		{
			TLink bus;
			uint64_t address = 0;
			size_t addressBits = 0;
		};
	public:
		virtual void attachSource(TLink& source, size_t maxNumSinks);
		virtual void attachSink(TLink& sink, uint64_t addressBase, size_t addressBits);

		const TLink& source() const;

		virtual void generate();
	private:
		Area m_area = "scl_TileLinkDemux";
		bool m_generated = false;
		size_t m_maxNumSinks = 0;
		TLink m_source;
		std::list<Sink> m_sink;
	};
}

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkDemux<TLink>::attachSource(TLink& source, size_t maxNumSinks)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_maxNumSinks == 0, "this module can support one source only");
		m_maxNumSinks = maxNumSinks;

		m_source = constructFrom(source);
		m_source <<= source;
	}

	template<TileLinkSignal TLink>
	inline void gtry::scl::TileLinkDemux<TLink>::attachSink(TLink& sink, uint64_t addressBase, size_t addressBits)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		HCL_DESIGNCHECK_HINT(m_maxNumSinks != 0, "attach source first");
		HCL_DESIGNCHECK_HINT(m_sink.size() < m_maxNumSinks, "sink limit reached");
		HCL_DESIGNCHECK_HINT(sink.chanA().source.width() >= m_source.chanA().source.width(), "source width too small");

		for (const Sink& s : m_sink)
			HCL_DESIGNCHECK_HINT(s.address != addressBase || s.addressBits != addressBits, "address conflict");

		Sink& s = m_sink.emplace_back();
		s.bus = constructFrom(sink);
		s.bus <<= sink;
		s.address = addressBase;
		s.addressBits = addressBits;
	}

	template<TileLinkSignal TLink>
	inline const TLink& gtry::scl::TileLinkDemux<TLink>::source() const
	{
		HCL_DESIGNCHECK_HINT(m_maxNumSinks != 0, "attach source first");
		return m_source;
	}

	template<TileLinkSignal TLink>
	inline void TileLinkDemux<TLink>::generate()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		m_generated = true;

		m_sink.sort([](Sink& a, Sink& b) {
			return a.addressBits < b.addressBits;
		});
		HCL_NAMED(m_source);
		HCL_NAMED(m_sink);

		const UInt& addr = m_source.chanA().address;

		// connect channel A
		ready(m_source.a) = '0';
		for (Sink& s : m_sink)
		{
			downstream(s.bus.a) = downstream(m_source.a);
			valid(s.bus.a) = '0';
			IF(addr.upper(addr.width() - s.addressBits) == s.address >> s.addressBits)
			{
				valid(s.bus.a) = '1';
				upstream(m_source.a) = upstream(s.bus.a);
			}
		}

		// connect channel D
		StreamArbiter<TileLinkChannelD> arbiter;
		for (Sink& s : m_sink)
			arbiter.attach(s.bus.d);
		m_source.d <<= arbiter.out();
		arbiter.generate();
	}

}

