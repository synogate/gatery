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

namespace gtry
{
	template<Signal T>
	class Reverse
	{
	public:
		Reverse() = default;
		Reverse(const Reverse&) = delete;
		Reverse(Reverse&& rhs);
		
		template<class TArg>
		Reverse(TArg&& arg) : m_obj(std::forward<TArg>(arg)) {}

		Reverse& operator = (const Reverse&) = delete;

		T& operator * () { return m_obj; }
		const T& operator * () const { return m_obj; }

		std::remove_reference_t<T>* operator ->() { return &m_obj; }
		const std::remove_reference_t<T>* operator ->() const { return &m_obj; }

	private:
		T m_obj;
	};

	struct ReversePlaceholder {};

	template<class T> T& downstream(T& val) { return val; }
	template<class T> ReversePlaceholder downstream(Reverse<T>& val) { return {}; }
	template<class T> ReversePlaceholder downstream(const Reverse<T>& val) { return {}; }
	template<CompoundSignal T> auto downstream(T& signal);
	template<CompoundSignal T> auto downstream(const T& signal);

	template<class T> ReversePlaceholder upstream(const T& val) { return {}; }
	template<class T> T& upstream(Reverse<T>& val) { return *val; }
	template<class T> const T& upstream(const Reverse<T>& val) { return *val; }
	template<CompoundSignal T> auto upstream(T& signal);
	template<CompoundSignal T> auto upstream(const T& signal);
}

namespace gtry
{
	namespace internal
	{
		template<typename T>
		T constructFromIfSignal(const T& signal)
		{
			if constexpr(Signal<T>)
				return constructFrom(signal);
			else
				return signal;
		}
	}

	template<Signal T>
	inline Reverse<T>::Reverse(Reverse&& rhs) :
		m_obj(internal::constructFromIfSignal(*rhs))
	{
		*rhs = m_obj;
	}

	template<CompoundSignal T>
	auto downstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
		}, boost::pfr::structure_tie(signal));
	}

	template<CompoundSignal T>
	auto downstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
		}, boost::pfr::structure_tie(signal));
	}

	template<CompoundSignal T>
	auto upstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
		}, boost::pfr::structure_tie(signal));
	}

	template<CompoundSignal T>
	auto upstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
		}, boost::pfr::structure_tie(signal));
	}

	template<class T> auto copy(const T& val) { return val; }
	template<class... T> auto copy(const std::tuple<T...>& tup)
	{
		return std::apply([](auto&&... args) {
			return std::make_tuple(copy(args)...);
		}, tup);
	}

	template<CompoundSignal T> using DownstreamSignal = decltype(copy(downstream(std::declval<T>())));
	template<CompoundSignal T> using UpstreamSignal = decltype(copy(upstream(std::declval<T>())));

	namespace internal
	{
		template<class T>
		struct is_reverse_signal : std::false_type {};

		template<Signal T>
		struct is_reverse_signal<Reverse<T>> : std::true_type {};
	}

	template<class T>
	concept ReverseSignal = internal::is_reverse_signal<T>::value;

	template<gtry::CompoundSignal T>
	T& connect(T& lhs, T& rhs)
	{
		using namespace gtry;
		downstream(lhs) = downstream(rhs);
		upstream(rhs) = upstream(lhs);
		return lhs;
	}

	inline namespace ops
	{
		template<CompoundSignal T>
		T& operator <<= (T& lhs, T& rhs) { return gtry::connect(lhs, rhs); }
	}
}
