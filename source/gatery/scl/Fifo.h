#pragma once
#include <gatery/frontend.h>

namespace gtry::scl
{
	template<typename TData>
	class Fifo
	{
		struct Port
		{
			BVec put;
			BVec get;

		};

	public:
		Fifo() = default;
		Fifo(size_t depth, TData ref) : Fifo() { setup(depth, std::move(ref)); }
		void setup(size_t depth, TData ref);

		void push(const TData& data, const Bit& valid);
		void pop(TData& data, const Bit& ready);

		const Bit& empty() const { return m_empty; }
		const Bit& full() const { return m_full; }

		Bit almostEmpty(const BVec& level) const { HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized"); return reg(m_size < level, '1'); }
		Bit almostFull(const BVec& level) const { HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized"); return reg(m_size >= level, '0'); }

	private:
		Memory<TData>	m_mem;
		BVec m_put;
		BVec m_get;
		BVec m_size;

		Bit m_full;
		Bit m_empty;

		TData m_readData;

		bool m_hasMem = false;
		bool m_hasPush = false;
		bool m_hasPop = false;
	};

	template<typename TData>
	inline void Fifo<TData>::setup(size_t depth, TData ref)
	{
		HCL_DESIGNCHECK_HINT(!m_hasMem, "fifo already initialized");
		m_hasMem = true;
		m_mem.setup(depth, std::move(ref));
		m_mem.noConflicts();

		const BitWidth ctrWidth = m_mem.addressWidth() + 1;
		m_put = ctrWidth;
		m_get = ctrWidth;
		m_size = m_put - m_get;
		HCL_NAMED(m_size);

		const Bit eq = m_put(0, -1) == m_get(0, -1);
		HCL_NAMED(eq);

		m_full = eq & (m_put.msb() != m_get.msb());
		m_empty = eq & (m_put.msb() == m_get.msb());

		m_full = reg(m_full, '0');
		m_empty = reg(m_empty, '1');
		HCL_NAMED(m_full);
		HCL_NAMED(m_empty);
	}

	template<typename TData>
	inline void Fifo<TData>::push(const TData& data, const Bit& valid)
	{
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPush, "fifo push port already constructed");
		m_hasPush = true;

		BVec put = m_put.getWidth();
		put = reg(put, 0);
		HCL_NAMED(put);

		IF(valid)
		{
			m_mem[put] = data;
			put += 1;
		}

		m_put = put;
		HCL_NAMED(m_put);
	}

	template<typename TData>
	inline void Fifo<TData>::pop(TData& data, const Bit& ready)
	{
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPop, "fifo pop port already constructed");
		m_hasPop = true;

		BVec get = m_get.getWidth();
		get = reg(get, 0);
		HCL_NAMED(get);

		data = m_mem[get];
		IF(ready)
			get += 1;

		m_get = get;
		HCL_NAMED(m_get);
	}
}
