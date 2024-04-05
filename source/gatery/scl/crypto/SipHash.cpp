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
#include "gatery/scl_pch.h"
#include "SipHash.h"

namespace gtry::scl
{
	SipHash::SipHash(size_t messageWordRounds, size_t finalizeRounds, size_t hashWidth) :
		m_messageWordRounds(messageWordRounds),
		m_finalizeRounds(finalizeRounds),
		m_hashWidth(hashWidth)
	{
		HCL_DESIGNCHECK_HINT((m_hashWidth == 64) | (m_hashWidth == 128), "SipHash is implemented for 64 and 128 bit output only");
	}

	void SipHash::initialize(SipHashState& state, const UInt& key)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "SipHashInit");

		state[0] = "x736f6d6570736575";
		state[1] = "x646f72616e646f6d";
		state[2] = "x6c7967656e657261";
		state[3] = "x7465646279746573";
		HCL_NAMED(state);

		HCL_DESIGNCHECK_HINT(key.size() == 128, "SipHash key must be 128bit wide");
		UInt k0 = key( 0, 64_b);
		UInt k1 = key(64, 64_b);
		HCL_NAMED(k0);
		HCL_NAMED(k1);

		state[0] ^= k0;
		state[1] ^= k1;
		state[2] ^= k0;
		state[3] ^= k1;

		if (m_hashWidth == 128)
			state[1] ^= 0xEE;
	}

	void SipHash::block(SipHashState& state, const UInt& block)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "SipHashBlock");

		HCL_DESIGNCHECK_HINT(block.size() % 64 == 0, "SipHash blocks need to be a multiple of 64 bit");

		UInt blockReg = block;
		HCL_NAMED(blockReg);

		for (size_t i = 0; i < blockReg.size() / 64; ++i)
		{
			state[3] ^= blockReg(i * 64, 64_b);
			for (size_t j = 0; j < m_messageWordRounds; ++j)
			{
				round(state);

				if (m_placeRegister)
				{
					blockReg = reg(blockReg);
					blockReg = reg(blockReg);
				}
			}
			state[0] ^= blockReg(i * 64, 64_b);
		}
	}

	UInt SipHash::finalize(SipHashState& state)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "SipHashFinalize");

		state[2] ^= (m_hashWidth == 64 ? 0xFF : 0xEE);

		UInt sipHashResult = ConstUInt(0, BitWidth{ m_hashWidth });
		HCL_NAMED(sipHashResult);

		for (size_t w = 0; w < m_hashWidth; w += 64)
		{
			for (size_t i = 0; i < m_finalizeRounds; ++i)
				round(state);

			sipHashResult(w, 64_b) = state[0] ^ state[1] ^ state[2] ^ state[3];
		}
		return sipHashResult;
	}

	void SipHash::sipOp(UInt& a, UInt& b, size_t aShift, size_t bShift)
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
		Area area{ "SipHashRound" };
		auto ent = area.enter();

		size_t pipelineAmount = 0;
		if (auto config = ent.config("pipeline"))
			pipelineAmount = config.as<size_t>();

		sipOp(state[0], state[1], 32, 13);
		sipOp(state[2], state[3], 0, 16);
		setName(state, "midstate");
		if (pipelineAmount > 1)
			state = pipestage(state);

		sipOp(state[2], state[1], 32, 17);
		sipOp(state[0], state[3], 0, 21);
		setName(state, "state");
		if (pipelineAmount > 0)
			state = pipestage(state);
	}

	UInt SipHash::pad(const UInt& block, size_t msgByteSize)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "SipHashPad");

		UInt paddedLength = ConstUInt(msgByteSize, 8_b);
		HCL_NAMED(paddedLength);

		BitWidth zeroPad{ (64 - (msgByteSize * 8 + 8)) % 64 };
		UInt paddedBlock = cat(paddedLength, zext(block(0, BitWidth{ msgByteSize * 8 }), +zeroPad));
		HCL_NAMED(paddedBlock);
		return paddedBlock;
	}

	std::tuple<UInt, size_t> sipHash(const UInt& block, const UInt& key, bool placeregister)
	{
		GroupScope entity(GroupScope::GroupType::ENTITY, "SipHash");


		//HCL_DESIGNCHECK_HINT(block.size() <= 56 && block.size() % 8 == 0, "no impl");
		HCL_DESIGNCHECK_HINT(block.size() % 8 == 0, "no impl");

		SipHash hash;
		hash.enableRegister(placeregister);

		UInt paddedBlock = hash.pad(block, block.size() / 8);
		HCL_NAMED(paddedBlock);

		SipHashState state;
		hash.initialize(state, key);
		hash.block(state, paddedBlock);
		return { hash.finalize(state), hash.latency(1, 64) };
	}
}
