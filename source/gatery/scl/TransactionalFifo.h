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

namespace gtry::scl
{
	template<Signal TData>
	class TransactionalFifo : public Fifo<TData>
	{
	public:
		TransactionalFifo() = default;
		TransactionalFifo(size_t minDepth, const TData& ref) : TransactionalFifo() { setup(minDepth, std::move(ref)); }

		void setup(size_t minDepth, const TData& ref);

		void commitPush();
		void commitPush(UInt cutoff);
		void rollbackPush();

		void commitPop();
		void rollbackPop();

	protected:
		virtual void generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet) override;
		virtual UInt generatePush(Memory<TData>& mem, const UInt& getAddr) override;
		virtual UInt generatePop(const Memory<TData>& mem, const UInt& putAddr) override;

	private:
		Bit m_pushCommit;
		UInt m_pushCutoff;
		Bit m_pushRollack;

		Bit m_popCommit;
		Bit m_popRollback;
	};

	template<Signal TData>
	inline void TransactionalFifo<TData>::setup(size_t minDepth, const TData& ref)
	{
		Fifo<TData>::setup(minDepth, ref);

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
	}

	template<Signal T>
	inline void TransactionalFifo<T>::rollbackPop()
	{
		m_popCommit = '0';
		m_popRollback = '1';
	}

	template<Signal TData>
	inline void TransactionalFifo<TData>::generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet)
	{
		HCL_ASSERT(!"no impl");
		// TDOD: implement cdc using handshake as graycode is insufficient for us
	}

	template<Signal TData>
	UInt TransactionalFifo<TData>::generatePush(Memory<TData>&mem, const UInt & get)
	{
		setName(Fifo<TData>::m_pushValid, "m_pushValid");
		setName(Fifo<TData>::m_pushData, "m_pushData");

		UInt put = get.width();
		put = reg(put, 0, { .clock = *Fifo<TData>::m_pushClock });

		IF(Fifo<TData>::m_pushValid)
		{
			mem[put(0, -1)] = Fifo<TData>::m_pushData;
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
		Fifo<TData>::m_pushFull = reg(put.msb() != get.msb() & put(0, -1) == get(0, -1), '0');
		setName(Fifo<TData>::m_pushFull, "m_pushFull");

		return putCheckpoint;
	}

	template<Signal TData>
	UInt TransactionalFifo<TData>::generatePop(const Memory<TData>& mem, const UInt& put)
	{
		setName(Fifo<TData>::m_popValid, "m_popValid");

		UInt get = put.width();
		get = reg(get, 0, { .clock = *Fifo<TData>::m_popClock });

		IF(Fifo<TData>::m_popValid)
			get += 1;

		UInt getCheckpoint = get.width();
		getCheckpoint = reg(getCheckpoint, 0, { .clock = *Fifo<TData>::m_popClock });
		HCL_NAMED(getCheckpoint);

		IF(m_popRollback)
			get = getCheckpoint;
		IF(m_popCommit)
			getCheckpoint = get;

		Fifo<TData>::m_peakData = reg(mem[get(0, -1)], { .clock = *Fifo<TData>::m_popClock, .allowRetimingBackward = true });

		Fifo<TData>::m_popSize = put - get;
		Fifo<TData>::m_popEmpty = reg(put.msb() == get.msb() & put(0, -1) == get(0, -1), '1');
		setName(Fifo<TData>::m_popEmpty, "m_popEmpty");

		return getCheckpoint;
	}
}
