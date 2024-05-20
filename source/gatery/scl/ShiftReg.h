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
	/**
	 * @brief Delay the input Signal by delay cycles. Possible implementations are shift registers or flip-flops.
	 * @param delay Number of cycles to delay the signal.
	 * @return Delayed signals.
	 */
	template<typename Signal>
	Signal delay(Signal signal, unsigned delay);

	/**
	 * @brief Delay the input Signal by delay cycles. This version allows for combinatorical change of delay.
	 * @param delay Number of cycles to delay the signal. Any change is reflected on the output immediately.
	 * @return Delayed signals.
	 */
	template<typename Signal>
	Signal delay(Signal signal, const UInt& delay);

	template<Signal TSig>
	class ShiftReg
	{
	public:
		ShiftReg() = default;
		ShiftReg(const TSig& in) { this->in(in); }
		ShiftReg(const ShiftReg&) = delete;
		ShiftReg(ShiftReg&&) = default;

		ShiftReg& in(const TSig& signal);

		const TSig& at(size_t index) const;
		const TSig& at(size_t index);
		const TSig at(UInt index);

		const TSig& operator [](size_t index) const { return at(index); }
		const TSig& operator [](size_t index) { return at(index); }
		const TSig operator [](const UInt& index) { return at(index); }

	protected:
		void expand(size_t size);

	private:
		Area m_area = { "scl_ShiftReg" };
		std::deque<TSig> m_chain;
	};
}

namespace gtry::scl
{
	template<typename Signal>
	Signal delay(Signal signal, unsigned delay) {
		for ([[maybe_unused]] auto i : utils::Range(delay))
			signal = reg(signal);
		return signal;
	}

	template<typename Signal>
	Signal delay(Signal signal, const UInt& delay)
	{
		Area ent{ "scl_delay", true };
		std::vector<Signal> chain(delay.width().count());
		chain[0] = signal;
		for (size_t i = 1; i < chain.size(); ++i)
			chain[i] = reg(chain[i - 1]);
		return mux(delay, chain);
	}

	template<Signal TSig>
	inline ShiftReg<TSig>& ShiftReg<TSig>::in(const TSig& signal)
	{
		auto ent = m_area.enter();

		if (m_chain.empty())
		{
			m_chain.emplace_back(signal);
			setName(m_chain.front(), "chain0");
		}
		else
			m_chain.front() = signal;

		return *this;
	}

	template<Signal TSig>
	inline const TSig& gtry::scl::ShiftReg<TSig>::at(size_t index) const
	{
		HCL_DESIGNCHECK(index < m_chain.size());
		return m_chain[index];
	}

	template<Signal TSig>
	inline const TSig& gtry::scl::ShiftReg<TSig>::at(size_t index)
	{
		auto ent = m_area.enter();
		expand(index + 1);
		return m_chain[index];
	}

	template<Signal TSig>
	inline const TSig gtry::scl::ShiftReg<TSig>::at(UInt index)
	{
		auto ent = m_area.enter();
		HCL_NAMED(index);
		expand(index.width().count());

		TSig out = mux(index, m_chain);
		HCL_NAMED(out);
		return out;
	}

	template<Signal TSig>
	inline void gtry::scl::ShiftReg<TSig>::expand(size_t size)
	{
		HCL_DESIGNCHECK(!m_chain.empty());
		while (size > m_chain.size())
		{
			m_chain.emplace_back(reg(m_chain.back()));
			setName(m_chain.back(), "chain" + std::to_string(m_chain.size() - 1));
		}
	}
}
