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

#include "../Counter.h"

namespace gtry::scl
{
	template<typename THash>
	class HashEngine
	{
	public:
		HashEngine() = default;
		HashEngine(size_t cyclesPerHash, size_t latencyCycles);

		void latency(size_t cycles);
		void throughput(size_t cyclesPerHash);

		void buildPipeline(THash& hash) const;
		void buildRoundProcessor(size_t startRound, THash& hash) const
		{
			const size_t numSections = std::max<size_t>(1, m_latency);
			for (size_t s = 0; s < numSections; ++s)
			{
				Counter roundCounter{ m_throughput };
				roundCounter.inc();

				THash state = constructFrom(hash);

				IF(roundCounter.isFirst())
					state = hash;

				const size_t roundsPerSection = THash::NUM_ROUNDS / numSections / m_throughput;
				for (size_t i = 0; i < roundsPerSection; ++i)
					state.round(roundCounter.value() * roundsPerSection + (s * roundsPerSection + i));

				if(m_latency > 0)
					state = reg(state);

				hash = state;
			}

		}

	private:
		size_t m_latency = 0;
		size_t m_throughput = 1;

	};

	template<typename THash>
	inline HashEngine<THash>::HashEngine(size_t cyclesPerHash, size_t latencyCycles)
	{
		latency(latencyCycles);
		throughput(cyclesPerHash);
	}

	template<typename THash>
	inline void HashEngine<THash>::latency(size_t cycles)
	{
		m_latency = cycles;
	}

	template<typename THash>
	inline void HashEngine<THash>::throughput(size_t cyclesPerHash)
	{
		m_throughput = cyclesPerHash;
	}

	template<typename THash>
	inline void HashEngine<THash>::buildPipeline(THash& hash) const
	{
		const size_t regInterval = m_latency ? THash::NUM_ROUNDS / m_latency : THash::NUM_ROUNDS + 1;

		for (size_t i = 0; i < THash::NUM_ROUNDS; ++i)
		{
			auto ent = Area{ "round" + std::to_string(i) }.enter();

			HCL_NAMED(hash);
			hash.round(i);

			if (i % regInterval == regInterval - 1)
				hash = reg(hash);
		}
	}
}
