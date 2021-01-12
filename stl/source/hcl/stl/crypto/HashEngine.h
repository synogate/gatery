#include <hcl/frontend.h>

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
