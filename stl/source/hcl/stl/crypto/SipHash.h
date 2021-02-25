#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	using SipHashState = std::array<BVec, 4>;

	class SipHash
	{
	public:
		SipHash(size_t messageWordRounds = 2, size_t finalizeRounds = 4, size_t hashWidth = 64);

		virtual void enableRegister(bool state) { m_placeRegister = state; }
		virtual size_t latency(size_t numBlocks, size_t blockSize) const { return m_placeRegister ? (m_messageWordRounds * (blockSize / 64) + m_finalizeRounds) : 0; }

		virtual void initialize(SipHashState& state, const BVec& key);
		virtual void block(SipHashState& state, const BVec& block);
		virtual BVec finalize(SipHashState& state);

		virtual void sipOp(BVec& a, BVec& b, size_t aShift, size_t bShift);
		virtual void round(SipHashState& state);
		virtual BVec pad(const BVec& block, size_t msgByteSize);

	protected:
		const size_t m_messageWordRounds;
		const size_t m_finalizeRounds;
		const size_t m_hashWidth;
		bool m_placeRegister = false;
	};

	std::tuple<BVec, size_t> sipHash(const BVec& block, const BVec& key, bool placeregister = true);

}
