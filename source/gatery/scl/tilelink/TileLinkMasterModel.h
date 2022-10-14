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

namespace gtry::scl
{
	class TileLinkMasterModel
	{
		struct Data
		{
			uint64_t mask;
			uint64_t value;
			uint64_t defined;
		};

		struct TransactionOut
		{
			TileLinkA::OpCode op;
			uint64_t address;
			uint64_t logByteSize;
			std::vector<Data> data;
		};

		struct TransactionIn
		{
			TileLinkD::OpCode op;
			std::vector<Data> data;
			bool error;
		};

	public:

		void init(std::string_view prefix, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth = 0_b, BitWidth sourceWidth = 0_b);

		void probability(float valid, float ready);

		SimFunction<TransactionIn> request(TransactionOut tx, const Clock& clk);

		SimFunction<std::tuple<uint64_t,uint64_t,bool>> get(uint64_t address, uint64_t logByteSize, const Clock &clk);
		SimFunction<bool> put(uint64_t address, uint64_t logByteSize, uint64_t data, const Clock &clk);

		auto &getLink() { return m_link; }

	protected:
		SimFunction<size_t> allocSourceId(const Clock &clk);

	private:
		size_t m_requestCurrent = 0;
		size_t m_requestNext = 0;

		TileLinkUL m_link;
		float m_validProbability = 1;
		float m_readyProbability = 1;
		std::vector<bool> m_sourceInUse;
		std::mt19937 m_rng;
		std::uniform_real_distribution<float> m_dis;
	};
}
