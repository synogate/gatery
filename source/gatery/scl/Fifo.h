#pragma once
#include <gatery/frontend.h>

namespace gtry::scl
{
	template<typename TData>
	class Fifo
	{
	public:
		Fifo() = default;
		Fifo(size_t depth, TData ref) : Fifo() { setup(depth, std::move(ref)); }
		void setup(size_t depth, TData ref);

		// NOTE: always push before pop for correct conflict resulution
		// TODO: fix above note by adding explicit write before read conflict resulution to bram
		void push(const TData& data, const Bit& valid);
		void pop(TData& data, const Bit& ready);

		const Bit& empty() const { return m_empty; }
		const Bit& full() const { return m_full; }

		Bit almostEmpty(const BVec& level) const;
		Bit almostFull(const BVec& level) const;

	private:
		Area m_area = "fifo";

		Memory<TData>	m_mem;
		BVec m_put;
		BVec m_get;
		BVec m_size;

		Bit m_full;
		Bit m_empty;

		bool m_hasMem = false;
		bool m_hasPush = false;
		bool m_hasPop = false;
	};

	template<typename TData>
	inline void Fifo<TData>::setup(size_t depth, TData ref)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(!m_hasMem, "fifo already initialized");
		m_hasMem = true;
		m_mem.setup(depth, std::move(ref));

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
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPush, "fifo push port already constructed");
		m_hasPush = true;

		sim_assert(!valid | !m_full) << "push into full fifo";

		BVec put = m_put.getWidth();
		put = reg(put, 0);
		HCL_NAMED(put);

		IF(valid)
		{
			m_mem[put(0, -1)] = data;
			put += 1;
		}

		m_put = put;
		HCL_NAMED(m_put);
	}

	template<typename TData>
	inline void Fifo<TData>::pop(TData& data, const Bit& ready)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPop, "fifo pop port already constructed");
		m_hasPop = true;

		sim_assert(!ready | !m_empty) << "pop from empty fifo";

		BVec get = m_get.getWidth();
		get = reg(get, 0);
		HCL_NAMED(get);

		IF(ready)
			get += 1;

		data = reg(m_mem[get(0, -1)]);

		m_get = get;
		HCL_NAMED(m_get);
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostEmpty(const BVec& level) const 
	{ 
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		return reg(m_size < level, '1'); 
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostFull(const BVec& level) const 
	{ 
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		return reg(m_size >= level, '0'); 
	}
}
