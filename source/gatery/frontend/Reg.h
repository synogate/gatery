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
#include "Signal.h"
#include "Compound.h"

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>
#include <boost/pfr.hpp>


namespace gtry
{
	class Clock;

	struct RegisterSettings {
		boost::optional<const Clock&> clock;
		bool allowRetimingBackward = false;
		bool allowRetimingForward = false;
	};

	static RegisterSettings retime{
		.allowRetimingBackward = true,
		.allowRetimingForward = true,
	};

	template<BaseSignal T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<BaseSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings = {});

	template<Signal T>
	T reg(const T& val, const RegisterSettings& settings = {});

	template<Signal T, typename Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings = {});

	namespace internal
	{
		SignalReadPort reg(SignalReadPort val, std::string_view name, std::optional<SignalReadPort> reset, const RegisterSettings& settings);
	}

	template<BaseSignal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return internal::reg(
			val.readPort(),
			val.getName(),
			std::nullopt,
			settings
		);
	}

	template<BaseSignal T, std::convertible_to<T> Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
	{
		NormalizedWidthOperands ops(val, T{ resetVal });

		return internal::reg(
			ops.lhs,
			val.getName(),
			ops.rhs,
			settings
		);
	}

	template<Signal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return internal::transformSignal(val, [&](const BaseSignal auto& sig) {
			return reg(sig, settings); // forward so it can have overloads
		});
	}

	template<Signal T, typename Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
	{
		return internal::transformSignal(val, resetVal, [&](const BaseSignal auto& sig, auto&& resetSig) {
			return reg(sig, resetSig, settings); // forward so it can have overloads
		});
	}

	template<Signal S>
	class Reg
	{
	public:
		Reg() = default;
		Reg(const Reg&) = delete;

		template<SignalValue Sv>
		Reg(const Sv& resetValue, const RegisterSettings& settings = {})
		{
			init(resetValue, settings);
		}

		template<SignalValue Sv>
		void init(const Sv& resetValue, const RegisterSettings& settings = {})
		{
			m_current = reg(m_next, resetValue, settings);
			m_next = m_set;
			m_set = m_current;
		}

		template<SignalValue Sv>
		Reg& operator = (Sv&& val) { m_set = val; return *this; }
		operator S() const { return m_current; }
		const S& next() const { return m_next; }
		const S& current() const { return m_current; }
		const S& combinatorial() const { return m_set; }

		void setName(std::string _name)
		{
			m_current.setName(_name);
			m_next.setName(_name + "_next");
		}
	private:
		S m_current;
		S m_set;
		S m_next;
	};
}
