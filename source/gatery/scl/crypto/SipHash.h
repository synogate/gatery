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

namespace gtry::scl
{
	using SipHashState = std::array<UInt, 4>;

	class SipHash
	{
	public:
		SipHash(size_t messageWordRounds = 2, size_t finalizeRounds = 4, size_t hashWidth = 64);

		virtual void enableRegister(bool state) { m_placeRegister = state; }
		virtual size_t latency(size_t numBlocks, size_t blockSize) const { return m_placeRegister ? 2 * (m_messageWordRounds * (blockSize / 64) + m_finalizeRounds) : 0; }

		virtual void initialize(SipHashState& state, const UInt& key);
		virtual void block(SipHashState& state, const UInt& block);
		virtual UInt finalize(SipHashState& state);

		virtual void sipOp(UInt& a, UInt& b, size_t aShift, size_t bShift);
		virtual void round(SipHashState& state);
		virtual UInt pad(const UInt& block, size_t msgByteSize);

	protected:
		const size_t m_messageWordRounds;
		const size_t m_finalizeRounds;
		const size_t m_hashWidth;
		bool m_placeRegister = false;
	};

	std::tuple<UInt, size_t> sipHash(const UInt& block, const UInt& key, bool placeregister = true);

}
