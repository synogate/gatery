/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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
#include "ModInstrFetch.h"
#include "../tilelink/tilelink.h"
#include "../tilelink/TileLinkFiFo.h"

namespace gtry::scl::rivet
{
	struct GenerationTag
	{
		BVec generationTag;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class InstrTileLink : public Base
	{
	public:
		void instrFetch(TileLinkU&& link)
		{
			m_link = constructFrom(link);
			link <<= m_link;
		}

		void instrFetchMetaFifoDepth(size_t depth) { m_fifoDepth = depth; }
		void instrFetchMetaFifoLatency(size_t latency) { m_fifoLatency = latency; }

	protected:

		auto instrFetch()
		{
			return [this](StreamSignal auto&& in)
				{
					StreamBroadcaster splitReq(move(in) | strm::attach(GenerationTag{ m_currentGeneration }));

					m_link.a <<= splitReq.bcastTo() |
						strm::transform([this](const InstrPtr& ip) 
							{
							TileLinkA ret = constructFrom(*m_link.a);
							ret.setupGet(resizeTo(ip.instrPtr, m_link.a->address.width()));
							return ret;
							}) |
						strm::reduceTo<TileLinkChannelA>() |
								name("instrFetchA");

							return move(*m_link.d) |
								name("instrFetchD") |
								strm::join(splitReq.bcastTo() | buffer() | name("InstrTileLink_fifo_out")) |
								strm::transform<strm::InsertPos::attach>([](const std::pair<TileLinkD, Nothing>& in) { return InstrOpcode{ in.first.data }; }) |
								strm::transform(&std::pair<TileLinkD, Nothing>::second) |
								strm::transform<strm::InsertPos::attach>([this](const Valid& in, const GenerationTag& gen) { return Valid{ in.valid & (gen.generationTag == m_currentGeneration) }; }) |
								strm::remove<GenerationTag>() |
								name("fetch")
								;
				};
		}

		template<StreamSignal T>
		void resultLoopBack(T&& result)
		{
			m_currentGeneration = reg(m_currentGeneration, 0);
			m_currentGeneration.lsb() ^= transfer(result) & get<JmpCtrl>(result).jmp;
			m_currentGeneration = m_currentGeneration | name("InstrTileLink_current_generation");

			Base::resultLoopBack(move(result));
		}


	private:

		auto buffer()
		{
			return [this](StreamSignal auto&& in)
				{
					if (m_fifoDepth == 1 && m_fifoLatency == 1)
						return strm::regDownstream(move(in));
					else
						return strm::fifo(move(in), m_fifoDepth, m_fifoLatency);
				};
		}

		size_t m_fifoDepth = 1;
		size_t m_fifoLatency = 1;

		TileLinkU m_link;
		BVec m_currentGeneration = 1_b;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::GenerationTag, generationTag);
