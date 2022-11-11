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
	void TileLinkMasterModel::init(std::string_view prefix, BitWidth addrWidth, BitWidth dataWidth, BitWidth sizeWidth, BitWidth sourceWidth)
	{
		tileLinkInit(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		pinIn(m_link, std::string{ prefix });

		m_sourceInUse.resize(sourceWidth.count(), false);
		m_rng.seed(std::random_device{}());

		Clock clk = ClockScope::getClk();

		// ready chaos monkey
		DesignScope::get()->getCircuit().addSimulationProcess([this, clk]() -> SimProcess {
			simu(valid(m_link.a)) = '0';

			simu(ready(*m_link.d)) = '0';
			while (true)
			{
				co_await OnClk(clk);
				simu(ready(*m_link.d)) = m_dis(m_rng) <= m_readyProbability;
			}
		});
	}

	void TileLinkMasterModel::probability(float valid, float ready)
	{
		m_validProbability = valid;
		m_readyProbability = ready;
	}

	SimFunction<TileLinkMasterModel::TransactionIn> TileLinkMasterModel::request(TransactionOut tx, const Clock& clk)
	{
		const float validProp = m_validProbability;
		const size_t myRequestId = m_requestNext++;
		while (myRequestId != m_requestCurrent)
			co_await m_requestCurrentChanged.wait();

		simu(valid(m_link.a)) = '0';
		simu(m_link.a->opcode) = (size_t)tx.op;
		simu(m_link.a->param) = 0;
		simu(m_link.a->address) = tx.address;
		simu(m_link.a->size) = tx.logByteSize;

		const size_t sourceId = co_await allocSourceId(clk);
		simu(m_link.a->source) = sourceId;

		co_await fork([=, this]()->SimProcess 
		{
			sim::DefaultBitVectorState state;
			state.resize(m_link.a->data.width().bits());

			for (const Data& d : tx.data)
			{
				state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, state.size(), d.value);
				state.insertNonStraddling(sim::DefaultConfig::DEFINED, 0, state.size(), d.defined);
				simu(m_link.a->data) = state;
				simu(m_link.a->mask) = d.mask;

				while (m_dis(m_rng) > validProp)
					co_await OnClk(clk);

				co_await scl::performTransfer(m_link.a, clk);
			}
			
			simu(m_link.a->opcode).invalidate();
			simu(m_link.a->param).invalidate();
			simu(m_link.a->address).invalidate();
			simu(m_link.a->size).invalidate();
			simu(m_link.a->source).invalidate();
			simu(m_link.a->mask).invalidate();
			simu(m_link.a->data).invalidate();

			m_requestCurrent++;
			m_requestCurrentChanged.notify_oldest();
		});

		TransactionIn ret;

		for (size_t i = 0; i < tx.inBurstBeats; ++i)
		{
			// ready set by chaos monkey
			do
				co_await scl::performTransferWait(*m_link.d, clk);
			while (simu((*m_link.d)->source) != sourceId);

			ret.data.push_back({
				.mask = 0,
				.value = simu((*m_link.d)->data).value(),
				.defined = simu((*m_link.d)->data).defined(),
			});
		}
		ret.error = simu((*m_link.d)->error);

		m_sourceInUse[sourceId] = false;
		co_return ret;
	}

	SimFunction<std::tuple<uint64_t,uint64_t,bool>> TileLinkMasterModel::get(uint64_t address, uint64_t logByteSize, const Clock &clk) 
	{
		TransactionOut req{
			.op = TileLinkA::Get,
			.address = address,
			.logByteSize = logByteSize,
		};

		const size_t bytePerBeat = m_link.a->mask.width().bits();
		const size_t byteSize = 1ull << logByteSize;
		req.inBurstBeats = (byteSize + bytePerBeat - 1) / bytePerBeat;

		auto [offset, mask] = prepareTransaction(req);
		req.data.resize(1);
		req.data[0].defined = 0;

		TransactionIn res = co_await request(req, clk);

		uint64_t value = 0;
		uint64_t defined = 0;
		for (size_t i = 0; i < res.data.size(); ++i)
		{
			const size_t w = m_link.a->data.width().bits();
			
			Data& d = res.data[i];
			value |= d.value >> offset << (i * w);
			defined |= d.defined >> offset << (i * w);
		}
		co_return std::tuple<uint64_t, uint64_t, bool>{value, defined, res.error};
	}

	SimFunction<bool> TileLinkMasterModel::put(uint64_t address, uint64_t logByteSize, uint64_t data, const Clock &clk) 
	{
		TransactionOut req{
			.op = TileLinkA::PutFullData,
			.address = address,
			.logByteSize = logByteSize,
			.inBurstBeats = 1,
		};

		auto [offset, mask] = prepareTransaction(req);

		for (Data& d : req.data)
		{
			d.defined = mask;
			d.value = (data << offset) & mask;
			data >>= m_link.a->data.width().bits();
		}

		TransactionIn res = co_await request(req, clk);
		co_return res.error;
	}

	SimFunction<size_t> TileLinkMasterModel::allocSourceId(const Clock &clk)
	{
		auto it = std::find(m_sourceInUse.begin(), m_sourceInUse.end(), false);
		while (it == m_sourceInUse.end()) {
			co_await OnClk(clk);
			it = std::find(m_sourceInUse.begin(), m_sourceInUse.end(), false);
		}

		*it = true;
		co_return size_t(it - m_sourceInUse.begin());
	}

	std::tuple<size_t, size_t> TileLinkMasterModel::prepareTransaction(TransactionOut& tx) const
	{
		const size_t bytePerBeat = m_link.a->mask.width().bits();
		const size_t byteSize = 1ull << tx.logByteSize;
		const size_t numBeats = (byteSize + bytePerBeat - 1) / bytePerBeat;
		const size_t byteOffset = tx.address & (bytePerBeat - 1);
		const size_t byteMask = utils::bitMaskRange(byteOffset, byteSize);
		const size_t bitOffset = byteOffset * 8;
		const size_t bitMask = utils::bitMaskRange(bitOffset, byteSize * 8);

		tx.data.resize(numBeats, {
			.mask = byteMask,
			.value = 0,
			.defined = bitMask
		});

		return { bitOffset, bitMask };
	}

	SimProcess TileLinkMasterModel::idle(size_t requestsPending)
	{
		while (m_requestNext - m_requestCurrent > requestsPending)
		{
			co_await m_requestCurrentChanged.wait();
			m_requestCurrentChanged.notify_oldest();
		}
	}
}
