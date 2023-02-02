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
#include "TileLinkMux.h"
#include "TileLinkDemux.h"

#include <boost/format.hpp>

namespace gtry::scl
{
	template<TileLinkSignal TLink>
	class TileLinkHub
	{
		enum class State { source, sink, generated };
	public:
		TileLinkHub();
		~TileLinkHub() noexcept(false);

		BitWidth sourceWidth();

		virtual void attachSource(TLink&& source) { attachSource(source); }
		virtual void attachSource(TLink& source);
		virtual TLink attachSource(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth);

		virtual void attachSink(TLink&& sink, uint64_t addressBase) { attachSink(sink, addressBase); }
		virtual void attachSink(TLink& sink, uint64_t addressBase);
		virtual void generate();
	protected:
		void enterSinkState();

	private:
		Area m_area = { "scl_TileLinkHub", true };
		State m_genState = State::source;

		TileLinkMux<TLink> m_mux;
		TileLinkDemux<TLink> m_demux;

		UInt m_OpenRequests = ConstUInt(0, 8_b);
		size_t m_SourceId = 0;

		BitWidth m_sourceW = 0_b;
	};
}

namespace gtry::scl
{
	UInt countOpenRequests(BitWidth w, TileLinkSignal auto& link, std::string name)
	{
		UInt counter = w;
		counter += transfer(link.a);
		counter -= transfer(*link.d);
		counter = reg(counter, 0);
		setName(counter, name);
		return counter;
	}

	template<TileLinkSignal TLink>
	inline TileLinkHub<TLink>::TileLinkHub()
	{
		m_area.leave();
	}

	template<TileLinkSignal TLink>
	inline TileLinkHub<TLink>::~TileLinkHub() noexcept(false)
	{
		HCL_DESIGNCHECK(m_genState == State::generated);
	}

	template<TileLinkSignal TLink>
	inline void TileLinkHub<TLink>::attachSource(TLink& source)
	{
		HCL_DESIGNCHECK_HINT(m_genState == State::source, "attach all sources first");
		auto ent = m_area.enter();

		m_OpenRequests |= countOpenRequests(m_OpenRequests.width(), source, 
			(boost::format("openRequestsSource%d") % m_SourceId++).str());
		m_mux.attachSource(source);
	}

	template<TileLinkSignal TLink>
	inline TLink TileLinkHub<TLink>::attachSource(BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth)
	{
		TLink ret;
		tileLinkInit(ret, addrWidth, dataWidth, sizeWidth, sourceWidth);
		attachSource(ret);
		return ret;
	}

	template<TileLinkSignal TLink>
	inline void TileLinkHub<TLink>::attachSink(TLink& sink, uint64_t addressBase)
	{
		auto ent = m_area.enter();
		enterSinkState();
		HCL_DESIGNCHECK_HINT(m_genState == State::sink, "already generated");
		m_OpenRequests |= countOpenRequests(m_OpenRequests.width(), sink,
			(boost::format("openRequestsSink%x") % addressBase).str());
		m_demux.attachSink(sink, addressBase);
	}

	template<TileLinkSignal TLink>
	inline void TileLinkHub<TLink>::generate()
	{
		HCL_DESIGNCHECK_HINT(m_genState == State::sink, "attach sources first, sinks second and call generate last");
		m_genState = State::generated;
		auto ent = m_area.enter();
		m_demux.generate();

		// TODO gatery frontend function to preserve signal and maybe make it visible to chip scope
		//pinOut(reg(m_OpenRequests, 0)).setName("LED");
	}

	template<TileLinkSignal TLink>
	inline BitWidth TileLinkHub<TLink>::sourceWidth()
	{
		auto ent = m_area.enter();
		enterSinkState();
		return m_sourceW;
	}

	template<TileLinkSignal TLink>
	inline void TileLinkHub<TLink>::enterSinkState()
	{
		if (m_genState == State::source)
		{
			m_genState = State::sink;
			TLink tunnle = m_mux.generate();
			HCL_NAMED(tunnle);
			m_demux.attachSource(tunnle);

			m_sourceW = tunnle.a->source.width();
		}
	}
}
