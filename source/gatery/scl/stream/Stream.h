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
#include "StreamConcept.h"
#include "metaSignals.h"

namespace gtry::scl::strm
{
	template<Signal PayloadT, Signal... Meta>
	struct Stream;

	template<Signal T, Signal... Meta>
	using RvStream = Stream<T, Ready, Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using VStream = Stream<T, Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using PacketStream = Stream<T, Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using RvPacketStream = Stream<T, Ready, Valid, Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using VPacketStream = Stream<T, Valid, Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using RsPacketStream = Stream<T, Ready, Sop, Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using SPacketStream = Stream<T, Sop, Eop, Meta...>;

	namespace internal
	{
		template<class T>
		struct is_stream_signal : std::false_type {};

		template<Signal... T>
		struct is_stream_signal<Stream<T...>> : std::true_type {};
	}


	template<typename T>
	concept Assignable = requires(T& a, const T& b) { a = b; };

	template<class T>
	concept BidirStreamSignal = StreamSignal<T> and !Assignable<T>;

	template<Signal PayloadT, Signal... Meta>
	struct StreamAssignabilityTestHelper
	{
		PayloadT data;
		std::tuple<Meta...> _sig;
	};

#ifdef __clang__
	#define GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD if constexpr (!Assignable<AssignabilityTestType>) return Self{}; else
#else
	#define GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD
#endif

	template<Signal PayloadT, Signal... Meta>
	struct Stream
	{
		using Payload = PayloadT;
		using Self = Stream<PayloadT, Meta...>;
#ifdef __clang__
		using AssignabilityTestType = StreamAssignabilityTestHelper<PayloadT, Meta...>; // Clang does not like querying things like assignability on an incomplete type (i.e. while still building the type).
#else		
		using AssignabilityTestType = Self;
#endif

		Payload data;
		std::tuple<Meta...> _sig;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		Payload* operator ->() { return &data; }
		const Payload* operator ->() const { return &data; }

		template<Signal T>
		static constexpr bool has();

		template<Signal T> inline auto add(T&& signal);
		template<Signal T> inline auto add(T&& signal) const requires (Assignable<AssignabilityTestType>);

		template<Signal T> constexpr T& get() { return std::get<T>(_sig); }
		template<Signal T> constexpr const T& get() const { return std::get<T>(_sig); }
		template<Signal T> constexpr void set(T&& signal) { get<T>() = std::forward<T>(signal); }

		auto transform(std::invocable<Payload> auto&& fun);
#ifdef __clang__
		template<typename unused = void>
		auto transform(std::invocable<Payload> auto&& fun) const requires (Assignable<AssignabilityTestType>);
#else		
		auto transform(std::invocable<Payload> auto&& fun) const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>);
#endif		

		template<StreamSignal T> T reduceTo();
		template<StreamSignal T> T reduceTo() const requires(Assignable<AssignabilityTestType>);

		template<StreamSignal T> explicit operator T ();
		template<StreamSignal T> explicit operator T () const requires(Assignable<AssignabilityTestType>);

		template<Signal T> auto remove();
		template<Signal T> auto remove() const requires(Assignable<AssignabilityTestType>);
		auto removeUpstream() { return remove<Ready>(); }
		auto removeUpstream() const requires(Assignable<AssignabilityTestType>) { 
			GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD
			return *this;
		}
		auto removeFlowControl() { return remove<Ready>().template remove<Valid>().template remove<Sop>(); }
		auto removeFlowControl() const requires(Assignable<AssignabilityTestType>) { 
			GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD
			return remove<Valid>().template remove<Sop>(); 
		}
	};
}

namespace gtry::scl::strm
{
	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline constexpr bool Stream<PayloadT, Meta...>::has()
	{
		bool ret = std::is_base_of_v<std::remove_reference_t<T>, std::remove_reference_t<PayloadT>>;
		if constexpr (sizeof...(Meta) != 0)
			ret |= (std::is_same_v<std::remove_reference_t<Meta>, std::remove_reference_t<T>> | ...);
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::add(T&& signal)
	{
		if constexpr (has<T>())
		{
			Stream ret;
			connect(ret.data, data);

			auto newMeta = std::apply([&](auto& ...meta) {
				auto fun = [&](auto& member) -> decltype(auto) {
					if constexpr (std::is_same_v<std::remove_cvref_t<decltype(member)>, T>)
						return std::forward<decltype(member)>(signal);
					else
						return std::forward<decltype(member)>(member);
				};
				return std::tie(fun(meta)...);
			}, _sig);

			downstream(ret._sig) = downstream(newMeta);
			upstream(newMeta) = upstream(ret._sig);
			return ret;
		}
		else
		{
			Stream<PayloadT, Meta..., T> ret;
			connect(*ret, data);
			ret.template get<T>() <<= signal;

			std::apply([&](auto& ...meta) {
				((ret.template get<std::remove_reference_t<decltype(meta)>>() <<= meta), ...);
			}, _sig);
			return ret;
		}
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::add(T&& signal) const requires (Assignable<AssignabilityTestType>)
	{
		if constexpr (has<T>())
		{
			Self ret = *this;
			ret.set(std::forward<T>(signal));
			return ret;
		}
		else
		{
			return Stream<PayloadT, Meta..., T>{
				data,
					std::apply([&](auto&... element) {
					return std::tuple(element..., std::forward<T>(signal));
				}, _sig)
			};
		}
	}

	template<Signal PayloadT, Signal ...Meta>
	inline auto Stream<PayloadT, Meta...>::transform(std::invocable<Payload> auto&& fun)
	{
		using gtry::connect;

		auto&& result = std::invoke(fun, data);
		Stream<std::remove_cvref_t<decltype(result)>, Meta...> ret;
		connect(ret.data, result);
		ret._sig <<= _sig;
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
#ifdef __clang__
	template<typename unused>
	inline auto Stream<PayloadT, Meta...>::transform(std::invocable<Payload> auto&& fun) const requires (Assignable<AssignabilityTestType>)
#else
	inline auto Stream<PayloadT, Meta...>::transform(std::invocable<Payload> auto&& fun) const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>)
#endif
	{
		auto&& result = std::invoke(fun, data);
		Stream<std::remove_cvref_t<decltype(result)>, Meta...> ret;
		ret.data = result;
		ret._sig = _sig;
		return ret;
	}

	template<typename ...T>
	void ignoreAll(T ...t) { }

	template<typename QueryMeta, Signal PayloadT, Signal ...Meta>
	int reductionChecker(const Stream<PayloadT, Meta...>&) { 
		static_assert(Stream<PayloadT, Meta...>::template has<QueryMeta>(), "Trying to reduce to a stream type that actually has additional meta flags in its signature.");
		return 0;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T>
	inline T Stream<PayloadT, Meta...>::reduceTo()
	{
		T ret;
		connect(ret.data, data);

		std::apply([&](auto&... meta) {
			ignoreAll(reductionChecker<std::remove_cvref_t<decltype(meta)>>(*this)...);
			((meta <<= std::get<std::remove_cvref_t<decltype(meta)>>(_sig)), ...);
		}, ret._sig);
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T>
	inline T Stream<PayloadT, Meta...>::reduceTo() const requires(Assignable<AssignabilityTestType>)
	{
		T ret{ data };
		std::apply([&](auto&... meta) {
			ignoreAll(reductionChecker<std::remove_cvref_t<decltype(meta)>>(*this)...);
			((ret.template get<std::remove_cvref_t<decltype(meta)>>() = meta), ...);
		}, ret._sig);
		return ret;
	}


	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T> 
	Stream<PayloadT, Meta...>::operator T ()
	{
		T ret{ data };

		auto assignIfExist = [&](auto&& ms) {
			if constexpr (T::template has<std::remove_cvref_t<decltype(ms)>>())
				ret.template get<std::remove_cvref_t<decltype(ms)>>() <<= ms;
		};

		std::apply([&](auto&... meta) {
			(assignIfExist(meta), ...);
		}, _sig);
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T> 
	Stream<PayloadT, Meta...>::operator T () const requires(Assignable<AssignabilityTestType>)
	{
		T ret{ data };

		auto assignIfExist = [&](auto&& ms) {
			if constexpr (T::template has<std::remove_cvref_t<decltype(ms)>>())
				ret.template get<std::remove_cvref_t<decltype(ms)>>() = ms;
		};

		std::apply([&](auto&... meta) {
			(assignIfExist(meta), ...);
		}, _sig);
		return ret;
	}

	namespace internal
	{
		template<class Tr>
		std::tuple<> remove_from_tuple(const std::tuple<>& t) { return {}; }

		template<class Tr, class T, class... To>
		auto remove_from_tuple(std::tuple<T, To...>& t)
		{
			auto head = std::tie(std::get<0>(t));
			auto tail = std::apply([](auto& tr, auto&... to) {
				return std::tie(to...);
			}, t);

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, Tr>)
			{
				return tail;
			}
			else if constexpr (sizeof...(To) > 0)
			{
				return std::tuple_cat(
					head,
					remove_from_tuple<Tr>(tail)
				);
			}
			else
			{
				return head;
			}
		}

		template<class Tr, class T, class... To>
		auto remove_from_tuple(const std::tuple<T, To...>& t)
		{
			auto head = std::tie(std::get<0>(t));
			auto tail = std::apply([](auto& tr, auto&... to) {
				return std::tie(to...);
			}, t);

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, Tr>)
			{
				return tail;
			}
			else if constexpr (sizeof...(To) > 0)
			{
				return std::tuple_cat(
					head,
					remove_from_tuple<Tr>(tail)
				);
			}
			else
			{
				return head;
			}
		}
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::remove()
	{
		auto metaRefs = internal::remove_from_tuple<T>(_sig);

		return std::apply([&](auto&... meta) {
			Stream<PayloadT, std::remove_cvref_t<decltype(meta)>...> ret;
			connect(ret.data, this->data);
			downstream(ret._sig) = downstream(metaRefs);
			upstream(metaRefs) = upstream(ret._sig);
			return ret;
		}, metaRefs);
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::remove() const requires(Assignable<AssignabilityTestType>)
	{
		auto metaRefs = internal::remove_from_tuple<T>(_sig);

		return std::apply([&](auto&... meta) {
			return Stream<PayloadT, std::remove_cvref_t<decltype(meta)>...>{
				this->data, metaRefs
			};
		}, metaRefs);
	}
}

namespace gtry {


	template<scl::strm::StreamSignal T>
	struct VisitCompound<T>
	{
		void operator () (T& a, const T& b, CompoundVisitor& v, size_t flags)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
					meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
					, ...);
			}, a._sig);

			VisitCompound<typename T::Payload>{}(a.data, b.data, v, flags);
		}

		void operator () (T& a, CompoundVisitor& v)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(meta, v), ...);
			}, a._sig);

			VisitCompound<typename T::Payload>{}(a.data, v);
		}

		void operator () (const T& a, const T& b, CompoundVisitor& v)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
					meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
					, ...);
			}, a._sig);

			VisitCompound<typename T::Payload>{}(a.data, b.data, v);
		}
	};
}

namespace gtry::scl {
	using strm::Stream;
	using strm::VStream;
	using strm::RvStream;
	using strm::PacketStream;
	using strm::RvPacketStream;
	using strm::VPacketStream;
	using strm::RsPacketStream;
	using strm::SPacketStream;
}


