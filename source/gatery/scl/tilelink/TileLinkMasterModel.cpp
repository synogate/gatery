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
#include "gatery/pch.h"
#include "tilelink.h"
#include "TileLinkMasterModel.h"

namespace gtry::scl
{
	TileLinkMasterModel::Handle::Handle(TileLinkMasterModel* model, size_t txid) :
		model(model),
		txid(txid)
	{}

	TileLinkMasterModel::Handle::~Handle()
	{
		model->closeHandle(txid);
	}

	TileLinkMasterModel::RequestState TileLinkMasterModel::Handle::state() const
	{
		return model->state(txid);
	}

	bool TileLinkMasterModel::Handle::busy() const
	{
		if (RequestState s = state(); s == RequestState::wait || s == RequestState::pending)
			return true;
		return false;
	}

	uint64_t TileLinkMasterModel::Handle::data() const
	{
		return model->getData(txid);
	}


	void TileLinkMasterModel::init(std::string_view prefix, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth)
	{
		tileLinkInit(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinIn(m_link, std::string{ prefix });

		m_sourceInUse.resize(sourceWidth.count(), false);
		m_rng.seed(std::random_device{}());

		Clock clk = ClockScope::getClk();

		// transmit process
		DesignScope::get()->getCircuit().addSimulationProcess([this, clk]() -> SimProcess {
			while (true)
			{
				simu(valid(m_link.a)) = '0';

				auto it = m_tx.end();
				while (true)
				{
					it = std::ranges::find(m_tx, RequestState::wait, &Request::state);
					if (it != m_tx.end())
						break;
					co_await WaitClk(clk);
				}

				it->source = allocSourceId();

				simu(m_link.a->opcode) = it->type;
				simu(m_link.a->param) = 0;
				simu(m_link.a->address) = it->address;
				simu(m_link.a->size) = it->size;
				simu(m_link.a->source) = it->source;

				uint64_t dataLength = m_link.a->mask.width().bits();
				uint64_t length = 1ull << it->size;
				uint64_t shift = length * (it->address & (length - 1));
				simu(m_link.a->mask) = (length - 1) << shift;
				simu(m_link.a->data) = it->data << (shift * 8);

				while (m_rng() % 100 < it->propValid)
					co_await WaitClk(clk);

				simu(valid(m_link.a)) = '1';
				do 
					co_await WaitClk(clk);
				while (simu(ready(m_link.a)) == '0');

				it->state = RequestState::pending;
			}
		});

		// receive process
		DesignScope::get()->getCircuit().addSimulationProcess([this, clk]() -> SimProcess {

			simu(ready(*m_link.d)) = '1';
			while (true)
			{
				if (simu(valid(*m_link.d)) == '0')
				{
					co_await WaitClk(clk);
					continue;
				}

				size_t source = simu((*m_link.d)->source);
				HCL_ASSERT(!m_sourceInUse[source]);

				auto it = std::ranges::find(m_tx, source, &Request::source);
				HCL_ASSERT(it != m_tx.end());

				m_sourceInUse[source] = false;

				if (simu((*m_link.d)->error) != '0')
					it->state = RequestState::fail;
				else
					it->state = RequestState::success;

				uint64_t length = 1ull << it->size;
				uint64_t shift = length * (it->address & (length - 1));
				it->data = (simu((*m_link.d)->data) >> shift) & (length - 1);

			}
		});
	}

	void TileLinkMasterModel::propability(size_t valid, size_t ready)
	{
		m_validPropability = valid;
		m_readyPropability = ready;
	}

	TileLinkMasterModel::Handle TileLinkMasterModel::get(uint64_t address, uint64_t logByteSize)
	{
		Request& r = m_tx.emplace_back(Request{
			.type = TileLinkA::Get,
			.address = address,
			.size = logByteSize,
			.propValid = m_validPropability,
			.propReady = m_readyPropability,
		});

		return Handle(this, m_txIdOffset + m_tx.size() - 1);
	}

	TileLinkMasterModel::Handle TileLinkMasterModel::put(uint64_t address, uint64_t logByteSize, uint64_t data)
	{
		Request& r = m_tx.emplace_back(Request{
			.type = TileLinkA::PutFullData,
			.address = address,
			.data = data,
			.size = logByteSize,
			.propValid = m_validPropability,
			.propReady = m_readyPropability,
		});

		return Handle(this, m_txIdOffset + m_tx.size() - 1);
	}

	size_t TileLinkMasterModel::allocSourceId()
	{
		auto it = std::find(m_sourceInUse.begin(), m_sourceInUse.end(), false);
		if (it == m_sourceInUse.end())
			throw std::runtime_error{ "no free source id, too many tile link requests in flight." };

		*it = true;
		return size_t(it - m_sourceInUse.begin());
	}

}
