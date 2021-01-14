#include <hcl/frontend.h>

#include "../Counter.h"

namespace hcl::stl
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

				THash state;
				// TDOO: copy bit width from hash to state

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
		const size_t regInterval = THash::NUM_ROUNDS / m_latency;

		for (size_t i = 0; i < THash::NUM_ROUNDS; ++i)
		{
			hash.round(i);

			if (i % regInterval == regInterval - 1)
				hash = reg(hash);
		}
	}
}
