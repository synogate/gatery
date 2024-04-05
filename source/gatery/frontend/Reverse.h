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

namespace gtry
{
	namespace scl::strm {
		struct Ready;
	}

	template<Signal T>
	class Reverse
	{
	public:
		using is_reverse_signal = std::true_type;

	public:
		Reverse() = default;
		Reverse(const Reverse&) = delete;
		Reverse(Reverse&& rhs);
		
		template<class TArg>
#ifdef __clang__
		requires (!std::same_as<TArg, std::tuple<const scl::strm::Ready&>&>)
#endif
		Reverse(TArg&& arg) : m_obj{std::forward<TArg>(arg)} {}

		Reverse& operator = (const Reverse&) = delete;
		Reverse& operator = (Reverse&&);

		T& operator * () { return m_obj; }
		const T& operator * () const { return m_obj; }

		std::remove_reference_t<T>* operator ->() { return &m_obj; }
		const std::remove_reference_t<T>* operator ->() const { return &m_obj; }

	private:
		T m_obj;
	};

	template<class T> T& downstream(T& val) { return val; }
	template<class T> decltype(auto) downstream(Reverse<T>& val);
	template<class T> decltype(auto) downstream(const Reverse<T>& val);
	template<CompoundSignal T> auto downstream(T& signal);
	template<CompoundSignal T> auto downstream(const T& signal);
	template<TupleSignal T> auto downstream(T& signal);
	template<TupleSignal T> auto downstream(const T& signal);

	struct ReversePlaceholder {};

	template<class T> ReversePlaceholder upstream(const T& val) { return {}; }
	template<class T> decltype(auto) upstream(Reverse<T>& val) { return downstream(*val); }
	template<class T> decltype(auto) upstream(const Reverse<T>& val) { return downstream(*val); }
	template<CompoundSignal T> auto upstream(T& signal);
	template<CompoundSignal T> auto upstream(const T& signal);
	template<TupleSignal T> auto upstream(T& signal);
	template<TupleSignal T> auto upstream(const T& signal);


	template<typename T>
	struct VisitCompound<Reverse<T>>
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (Reverse<T>& a, const Reverse<T>& b, Visitor& v)
		{
			v.reverse();
			VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v);
			v.reverse();
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (Reverse<T>& a, Visitor& v)
		{
			v.reverse();
			VisitCompound<std::remove_cvref_t<T>>{}(*a, v);
			v.reverse();
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const Reverse<T>& a, const Reverse<T>& b, Visitor& v)
		{
			v.reverse();
			VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v);
			v.reverse();
		}
	};

}

namespace gtry
{
	template<class T> decltype(auto) downstream(Reverse<T>& val)
	{
		return upstream(*val);
	}

	template<class T> decltype(auto) downstream(const Reverse<T>& val)
	{
		return upstream(*val);
	}

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
		m_obj = std::move(rhs.m_obj); 
	}

	template<Signal T>
	inline Reverse<T>& Reverse<T>::operator=(Reverse&& rhs)
	{
		m_obj = std::move(rhs.m_obj);
		return *this;
	}


	template<CompoundSignal T>
	auto downstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
		}, structure_tie(signal));
	}

	template<CompoundSignal T>
	auto downstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
		}, structure_tie(signal));
	}

	template<TupleSignal T>
	auto downstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
			}, signal);
	}

	template<TupleSignal T>
	auto downstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(downstream(args))...>{downstream(args)...};
			}, signal);
	}

	template<CompoundSignal T>
	auto upstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
		}, structure_tie(signal));
	}

	template<CompoundSignal T>
	auto upstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
		}, structure_tie(signal));
	}

	template<TupleSignal T>
	auto upstream(T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
			}, signal);
	}

	template<TupleSignal T>
	auto upstream(const T& signal)
	{
		return std::apply([](auto&&... args) {
			return std::tuple<decltype(upstream(args))...>{upstream(args)...};
			}, signal);
	}

	/// Copies a tuple of references to turn the references into proper lvalues. 
	/// @details This sometimes needs to be used in combination with upstream/downstream since those return tuples of references.
	template<class T> auto copy(const T& val) { return val; }
	/// Copies a tuple of references to turn the references into proper lvalues. 
	/// @details This sometimes needs to be used in combination with upstream/downstream since those return tuples of references.
	template<class... T> auto copy(const std::tuple<T...>& tup)
	{
		return std::apply([](auto&&... args) {
			return std::make_tuple(copy(args)...);
		}, tup);
	}

	template<CompoundSignal T> using DownstreamSignal = decltype(copy(downstream(std::declval<T>())));
	template<CompoundSignal T> using UpstreamSignal = decltype(copy(upstream(std::declval<T>())));

	template<gtry::Signal T>
	void connect(T& lhs, T& rhs)
	{
		using namespace gtry;
		downstream(lhs) = downstream(rhs);
		upstream(rhs) = upstream(lhs);
	}

	template<class Ta, class Tb>
	concept Connectable = requires(Ta& a, Tb& b) { connect(a, b); };

	template<class Ta, class Tb>
	requires (Connectable<Ta, Tb> and not BaseSignal<Ta>)
	void operator <<= (Ta&& lhs, Tb&& rhs) { connect(lhs, rhs); }
}
