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
#include "Fifo.h"
#include "stream/StreamConcept.h" 
#include "stream/metaSignals.h"

namespace gtry::scl
{
	template<Signal TData>
	class TransactionalFifo : public Fifo<TData>
	{
	public:
		TransactionalFifo() = default;
		explicit TransactionalFifo(size_t minDepth, const TData& ref = TData{}, FifoLatency latency = FifoLatency::DontCare()) : TransactionalFifo() { setup(minDepth, std::move(ref), latency); }

		void setup(size_t minDepth, const TData& ref = TData{}, FifoLatency latency = FifoLatency::DontCare());

		void commitPush();
		void commitPush(UInt cutoff);
		void rollbackPush();

		void commitPop();
		void rollbackPop();

	protected:
		virtual void generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet) override;
		virtual UInt generatePush(Memory<TData>& mem, UInt getAddr) override;
		virtual UInt generatePop(const Memory<TData>& mem, UInt putAddr) override;

	private:
		Bit m_pushCommit;
		UInt m_pushCutoff;
		Bit m_pushRollack;

		Bit m_popCommit;
		Bit m_popRollback;

		bool m_hasPopCommit = false;
		bool m_hasPushCommit = false;
	};

	template<Signal TData>
	inline void TransactionalFifo<TData>::setup(size_t minDepth, const TData& ref, FifoLatency latency)
	{
		Fifo<TData>::setup(minDepth, ref, latency);

		m_pushCommit = '0';
		m_pushRollack = '0';
		m_popCommit = '0';
		m_popRollback = '0';

		m_pushCutoff = ConstUInt(0, BitWidth::count(Fifo<TData>::depth()) + 1);
	}

	template<Signal T>
	inline void TransactionalFifo<T>::commitPush()
	{
		m_pushCommit = '1';
		m_pushRollack = '0';
		m_hasPushCommit = true;
	}

	template<Signal TData>
	inline void TransactionalFifo<TData>::commitPush(UInt cutoff)
	{
		commitPush();
		m_pushCutoff = cutoff;
	}

	template<Signal T>
	inline void TransactionalFifo<T>::rollbackPush()
	{
		m_pushCommit = '0';
		m_pushRollack = '1';
	}

	template<Signal T>
	inline void TransactionalFifo<T>::commitPop()
	{
		m_popCommit = '1';
		m_popRollback = '0';
		m_hasPopCommit = true;
	}

	template<Signal T>
	inline void TransactionalFifo<T>::rollbackPop()
	{
		m_popCommit = '0';
		m_popRollback = '1';
	}

	namespace internal {
		void generateCDCReqAck(const UInt& inData, UInt& outData, const Clock& inDataClock, const Clock& outDataClock);
	}

	template<Signal TData>
	inline void TransactionalFifo<TData>::generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet)
	{
		internal::generateCDCReqAck(pushPut, popPut, *Fifo<TData>::m_pushClock, *Fifo<TData>::m_popClock);
		internal::generateCDCReqAck(popGet, pushGet, *Fifo<TData>::m_popClock, *Fifo<TData>::m_pushClock);
	}

	template<Signal TData>
	UInt TransactionalFifo<TData>::generatePush(Memory<TData>&mem, UInt get)
	{
		ClockScope clk{ *Fifo<TData>::m_pushClock };
		if (!m_hasPushCommit)
			commitPush();

		setName(Fifo<TData>::m_pushValid, "m_pushValid");
		setName(Fifo<TData>::m_pushData, "m_pushData");
		HCL_NAMED(m_pushCutoff);
		HCL_NAMED(m_pushRollack);
		HCL_NAMED(m_pushCommit);

		UInt put = get.width();
		put = reg(put, 0, { .clock = *Fifo<TData>::m_pushClock });
		HCL_NAMED(put);

		IF(Fifo<TData>::m_pushValid)
		{
			mem[put(0, -1_b)] = Fifo<TData>::m_pushData;
			put += 1;
		}

		UInt putCheckpoint = put.width();
		putCheckpoint = reg(putCheckpoint, 0, { .clock = *Fifo<TData>::m_pushClock });
		HCL_NAMED(putCheckpoint);

		IF(m_pushRollack)
			put = putCheckpoint;
		IF(m_pushCommit)
		{
			put -= m_pushCutoff;
			putCheckpoint = put;
		}

		Fifo<TData>::m_pushSize = put - get;
		Fifo<TData>::m_pushFull = reg(put.msb() != get.msb() & put(0, -1_b) == get(0, -1_b), '0');
		setName(Fifo<TData>::m_pushFull, "m_pushFull");

		return putCheckpoint;
	}

	template<Signal TData>
	UInt TransactionalFifo<TData>::generatePop(const Memory<TData>& mem, UInt put)
	{
		ClockScope clk{ *Fifo<TData>::m_popClock };
		if (!m_hasPopCommit)
			commitPop();

		setName(Fifo<TData>::m_popValid, "m_popValid");
		HCL_NAMED(m_popRollback);
		HCL_NAMED(m_popCommit);

		UInt get = put.width();
		get = reg(get, 0, { .clock = *Fifo<TData>::m_popClock });
		HCL_NAMED(get);

		IF(Fifo<TData>::m_popValid)
			get += 1;

		UInt getCheckpoint = get.width();
		getCheckpoint = reg(getCheckpoint, 0, { .clock = *Fifo<TData>::m_popClock });
		HCL_NAMED(getCheckpoint);

		IF(m_popRollback)
			get = getCheckpoint;
		IF(m_popCommit)
			getCheckpoint = get;

		Fifo<TData>::m_peekData = reg(mem[get(0, -1_b)], { .clock = *Fifo<TData>::m_popClock, .allowRetimingBackward = true });

		Fifo<TData>::m_popSize = put - get;
		Fifo<TData>::m_popEmpty = reg(put.msb() == get.msb() & put(0, -1_b) == get(0, -1_b), '1');
		setName(Fifo<TData>::m_popEmpty, "m_popEmpty");

		return getCheckpoint;
	}
}
