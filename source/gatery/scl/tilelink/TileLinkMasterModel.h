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
		enum class RequestState
		{
			wait, pending, success, fail
		};

		struct Request
		{
			TileLinkA::OpCode type;
			RequestState state = RequestState::pending;
			uint64_t address;
			uint64_t data = 0;
			size_t size;
			size_t source = ~0u;
			size_t propValid;
			size_t propReady;
		};

		class Handle
		{
			TileLinkMasterModel* model;
			size_t txid;
		public:
			Handle(TileLinkMasterModel* model, size_t txid);
			Handle(const Handle&) = delete;
			Handle(Handle&&) = default;
			~Handle();

			RequestState state() const;
			bool busy() const;
			uint64_t data() const;
		};

	public:

		void init(std::string_view prefix, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth = 0_b, BitWidth sourceWidth = 0_b);

		void propability(size_t valid, size_t ready);

		Handle get(uint64_t address, uint64_t logByteSize);
		Handle put(uint64_t address, uint64_t logByteSize, uint64_t data);

		RequestState state(size_t txid) const;
		uint64_t getData(size_t txid) const;
		void closeHandle(size_t txid);

	protected:
		size_t allocSourceId();
		Request& req(size_t txid);
		const Request& req(size_t txid) const;

	private:
		TileLinkUH m_link;
		size_t m_txIdOffset = 0;
		size_t m_validPropability = 100;
		size_t m_readyPropability = 100;
		std::deque<Request> m_tx;
		std::vector<bool> m_sourceInUse;
		std::mt19937 m_rng;
	};
}
