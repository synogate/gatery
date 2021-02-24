#include "SipHash.h"

namespace hcl::stl
{
	SipHash::SipHash(size_t messageWordRounds, size_t finalizeRounds, size_t hashWidth) :
		m_messageWordRounds(messageWordRounds),
		m_finalizeRounds(finalizeRounds),
		m_hashWidth(hashWidth)
	{
		HCL_DESIGNCHECK_HINT(m_hashWidth == 64 | m_hashWidth == 128, "SipHash is implemented for 64 and 128 bit output only");
	}

	void SipHash::initialize(SipHashState& state, const BVec& key)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("SipHashInit");

		HCL_NAMED(state);
		state[0] = "x736f6d6570736575";
		state[1] = "x646f72616e646f6d";
		state[2] = "x6c7967656e657261";
		state[3] = "x7465646279746573";
		
		HCL_DESIGNCHECK_HINT(key.size() == 128, "SipHash key must be 128bit wide");
		BVec k0 = swapEndian(key(64, 64));
		BVec k1 = swapEndian(key(0, 64));
		HCL_NAMED(k0);
		HCL_NAMED(k1);

		state[0] ^= k0;
		state[1] ^= k1;
		state[2] ^= k0;
		state[3] ^= k1;

		if (m_hashWidth == 128)
			state[1] ^= 0xEE;
	}

	void SipHash::block(SipHashState& state, const BVec& block)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("SipHashBlock");

		HCL_DESIGNCHECK_HINT(block.size() % 64 == 0, "SipHash blocks need to be a multiple of 64 bit");

		for (size_t i = 0; i < block.size() / 64; ++i)
		{
			BVec msgWord = swapEndian(block(i * 64, 64));
			HCL_NAMED(msgWord);

			state[3] ^= msgWord;
			for (size_t j = 0; j < m_messageWordRounds; ++j)
			{
				sim_debug() << "state0: " << state[0];
				sim_debug() << "state1: " << state[1];
				sim_debug() << "state2: " << state[2];
				sim_debug() << "state3: " << state[3];
				round(state);
			}
			state[0] ^= msgWord;
		}
	}

	BVec SipHash::finalize(SipHashState& state)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("SipHashFinalize");

		state[2] ^= (m_hashWidth == 64 ? 0xFF : 0xEE);

		BVec sipHashResult = ConstBVec(0, m_hashWidth);
		HCL_NAMED(sipHashResult);

		for (size_t w = 0; w < m_hashWidth; w += 64)
		{
			for (size_t i = 0; i < m_finalizeRounds; ++i)
				round(state);

			sipHashResult(w, 64) = state[0] ^ state[1] ^ state[2] ^ state[3];
		}
		return sipHashResult;
	}

	void SipHash::sipOp(BVec& a, BVec& b, size_t aShift, size_t bShift)
	{
		a += b;
		b = rotl(b, bShift) ^ a;

		if(aShift)
			a = rotl(a, aShift);

		if (m_placeRegister)
		{
			a = reg(a);
			b = reg(b);
		}
	}

	void SipHash::round(SipHashState& state)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("SipHashRound");

		sipOp(state[0], state[1], 32, 13);
		sipOp(state[2], state[3], 0, 16);
		sipOp(state[2], state[1], 32, 17);
		sipOp(state[0], state[3], 0, 21);
	}

	BVec SipHash::pad(const BVec& block, size_t msgByteSize)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("SipHashPad");

		BVec paddedLength = ConstBVec(msgByteSize, 8);
		HCL_NAMED(paddedLength);

		size_t zeroPad = 64 - ((msgByteSize * 8 + 8) % 64);
		BVec paddedBlock = cat(paddedLength, zext(block(0, msgByteSize*8), zeroPad));
		HCL_NAMED(paddedBlock);
		return paddedBlock;
	}
}
