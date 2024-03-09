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
#include "ConstructFrom.h"

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>
#include <gatery/compat/boost_pfr.h>


namespace gtry
{
/**
 * @addtogroup gtry_frontend
 * @{
 */

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
		SignalReadPort reg(SignalReadPort val, std::optional<SignalReadPort> reset, const RegisterSettings& settings);
	}

	template<BaseSignal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return internal::reg(
			val.readPort(),
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
			ops.rhs,
			settings
		);
	}

	template<Signal T>
	T reg(const T& val, const RegisterSettings& settings)
	{
		return internal::transformSignal(val, [&](const auto& sig) {
			if constexpr (!Signal<decltype(sig)>)
				return sig;
			else
				return reg(sig, settings); // forward so it can have overloads
		});
	}

	template<Signal T, typename Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings& settings)
	{
		return internal::transformSignal(val, resetVal, [&](const auto& sig, auto&& resetSig) {
			if constexpr (!Signal<decltype(sig)>)
				return resetSig;
			else
				return reg(sig, resetSig, settings); // forward so it can have overloads
		});
	}

	struct RegFactory
	{
		RegisterSettings settings;
		auto operator() (Signal auto&& sig) { return reg(sig, settings); }
	};

	/**
	 * @brief A special overload of reg that is used for the pipe operator. It is implimented with the RegFactory class to support.
	 *			Cases where we would like to initialize a signal as a register using Signal a = reg(); syntax.
	*/
	inline RegFactory reg(const RegisterSettings& settings = {}) { return { settings }; }

	template<Signal S>
	class Reg
	{
	public:
		Reg() = default;
		Reg(const Reg &other) {
			if (other.m_initialized)
				init(other.m_resetValue, other.m_settings);
		}

		template<SignalValue Sv>
		Reg(const Sv& resetValue, const RegisterSettings& settings = {})
		{
			init(resetValue, settings);
		}

		void constructFrom(const S& templateValue, const RegisterSettings& settings = {})
		{
			m_settings = settings;
			m_resetValue = dontCare(templateValue);
			m_set = gtry::constructFrom(templateValue);
			m_next = m_set;
			m_current = reg(m_next, m_settings);
			m_set = m_current;
			m_initialized = true;
		}

		template<SignalValue Sv>
		void init(const Sv& resetValue, const RegisterSettings& settings = {})
		{
			m_settings = settings;
			m_resetValue = resetValue;
			m_set = gtry::constructFrom(m_resetValue);
			m_next = m_set;
			m_current = reg(m_next, m_resetValue, m_settings);
			m_set = m_current;
			m_initialized = true;
		}

		template<SignalValue Sv>
		Reg& operator = (Sv&& val) {
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			m_set = val; 
			return *this; 
		}
		operator S() const { 
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			return m_current; 
		}
		const S& next() const { 
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			return m_next; 
		}
		const S& current() const { 
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			return m_current; 
		}
		const S& combinatorial() const { 
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			return m_set; 
		}

		void setName(std::string _name)
		{
			HCL_DESIGNCHECK_HINT(m_initialized, "Register class must be initialized before use.");
			gtry::setName(m_current, _name);
			gtry::setName(m_next, _name + "_next");
		}
	private:
		bool m_initialized = false;
		S m_resetValue;
		S m_current;
		S m_set;
		S m_next;
		RegisterSettings m_settings;
	};


/// @}

}
