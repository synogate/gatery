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
	public:
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
			uint64_t inBurstBeats;
			std::vector<Data> data;
			std::optional<uint64_t> source;
			bool freeSource = true;
		};

		struct TransactionIn
		{
			TileLinkD::OpCode op;
			std::vector<Data> data;
			bool error;
			uint64_t source;
		};

	public:

		void init(std::string_view prefix, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth = 0_b, BitWidth sourceWidth = 0_b);
		void init(std::string_view prefix, const TileLinkUB& tlub);
		void initAndDrive(std::string_view prefix, TileLinkUL&& slave);

		void probability(float valid, float ready, uint32_t seed = 1337);

		SimFunction<TransactionIn> request(TransactionOut tx, const Clock& clk);
		void freeSourceId(const size_t& sourceId);
		SimProcess idle(size_t requestsPending = 0);


		SimFunction<std::tuple<uint64_t,uint64_t,bool>> get(uint64_t address, uint64_t logByteSize, const Clock &clk);
		SimFunction<bool> put(uint64_t address, uint64_t logByteSize, uint64_t data, const Clock &clk);

		auto &getLink() { return m_link; }

		TransactionOut setupGet(uint64_t address, uint64_t logByteSize);
		TransactionOut setupPut(uint64_t address, uint64_t logByteSize, uint64_t data);
		std::tuple<uint64_t, uint64_t, bool> extractResult(const TransactionIn& res, TransactionOut req);

	protected:
		SimFunction<size_t> allocSourceId(const Clock &clk);

		std::tuple<size_t, size_t> prepareTransaction(TransactionOut& tx) const;

	private:
		size_t m_requestCurrent = 0;
		size_t m_requestNext = 0;
		Condition m_requestCurrentChanged;

		TileLinkUB m_link;
		float m_validProbability = 1;
		float m_readyProbability = 1;
		std::set<size_t> m_sourceInUse;
		std::uint64_t m_numSourceIDsTotal;
		std::mt19937 m_rng = std::mt19937{ 1337 };
		std::uniform_real_distribution<float> m_dis;
	};
}
