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
	// TODO: resolve which of these is corret

	#ifdef __clang__
	#define GTRY_STREAM_ASSIGNABLE Assignable<StreamAssignabilityTestHelper<PayloadT, Meta...>>
	#define GTRY_STREAM_BIDIR BidirStreamSignal<StreamAssignabilityTestHelper<PayloadT, Meta...>>
	#else
	#define GTRY_STREAM_ASSIGNABLE Assignable<Self>
	#define GTRY_STREAM_BIDIR BidirStreamSignal<Self>
	#endif
/*
#ifdef __clang__
	#define GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD if constexpr (!Assignable<AssignabilityTestType>) return Self{}; else
#else
	#define GTRY_STRM_CLANG_RETURN_CONSTEXPR_GUARD
#endif
*/
	template<Signal PayloadT, Signal... Meta>
	struct Stream
	{
		using Payload = PayloadT;
		using Self = Stream<PayloadT, Meta...>;

		Payload data;
		std::tuple<Meta...> _sig;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		Payload* operator ->() { return &data; }
		const Payload* operator ->() const { return &data; }

		template<Signal T>
		static constexpr bool has();

		template<StreamSignal T> explicit operator T ();
		template<StreamSignal T> explicit operator T () const requires(GTRY_STREAM_ASSIGNABLE);

	};

	template<Signal AddT, Signal PayloadT, Signal ...MetaT>	auto attach(Stream<PayloadT, MetaT...>&& stream, AddT&& signal);
	template<Signal AddT, Signal PayloadT, Signal ...MetaT>	auto attach(const Stream<PayloadT, MetaT...>& stream, AddT&& signal);
	/**
	* @brief Adds the payload of a stream as a metadata field to another stream.
	* @details The streams are synchronized to wait for each other and, in case of packet streams, to keep the metadata signal stable for the entire duration of a packet.
	*/
	template<Signal AddPayloadT, Signal ...AddMetaT, Signal PayloadT, Signal ...Meta> auto attach(Stream<PayloadT, Meta...>&& stream, Stream<AddPayloadT, AddMetaT...>&& metaStream);
	template<Signal AddT> auto attach(AddT&& signal);

	/**
	* @brief Adds the payload of a stream as a metadata field to another stream, encapsulating it in a wrapper struct to make it identifyable with the ::get, ::has, and ::remove member functions.
	*/
	template<Signal Wrapper, StreamSignal T, Signal PayloadT, Signal ...MetaT> auto attachAs(Stream<PayloadT, MetaT...>&& stream, T&& metaStream);
	template<Signal Wrapper, StreamSignal T> auto attachAs(T&& metaStream);

	/**
	 * @brief inverse of attach
	 */
	template<Signal T, StreamSignal StreamT>
	auto remove(StreamT&& stream);

	template<Signal SigT>
	auto remove();

	template<StreamSignal StreamT>
	auto removeUpstream(StreamT&& stream);
	auto removeUpstream();

	template<StreamSignal StreamT>
	auto removeFlowControl(StreamT&& stream);
	auto removeFlowControl();

	template<Signal T, StreamSignal StreamT> constexpr T& get(StreamT&& stream);
	template<Signal T, StreamSignal StreamT> constexpr const T& get(const StreamT& stream);
	template<Signal T> auto get();

	/**
	 * @brief Transforms the stream payload using a provided function.
	 *
	 * This function applies a transformation function to the stream, allowing for modification
	 * of the stream's payload. The transformation function should accept the payload
	 * as an argument and return the new payload.
	 */
	template<StreamSignal StreamT, typename FunT>
	auto transform(StreamT&& stream, FunT&& fun);

	template<typename FunT>
	auto transform(FunT&& fun);

	/**
	 * @brief Cast Stream into a different Stream type that has the same payload but a subset of the meta signals.
	 * 
	 * This function is also used to reorder the meta signals, as a work around until attach can insert meta signal in sorted order.
	 */
	template<StreamSignal T, StreamSignal srcT>
	T reduceTo(srcT&& stream);

	template<StreamSignal SigT>
	auto reduceTo();
}

namespace gtry::scl::strm
{
	// adding deduction guide for clang compatibility
	template<typename PayloadT, typename... Meta>
	Stream(PayloadT, std::tuple<Meta...>) -> Stream<PayloadT, Meta...>;

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline constexpr bool Stream<PayloadT, Meta...>::has()
	{
		bool ret = std::is_base_of_v<std::remove_reference_t<T>, std::remove_reference_t<PayloadT>>;
		if constexpr (sizeof...(Meta) != 0)
			ret |= (std::is_same_v<std::remove_reference_t<Meta>, std::remove_reference_t<T>> | ...);
		return ret;
	}

	template<Signal T, StreamSignal StreamT>
	constexpr T& get(StreamT&& stream)
	{
		return std::get<T>(stream._sig);
	}

	template<Signal T, StreamSignal StreamT>
	constexpr const T& get(const StreamT& stream)
	{
		return std::get<T>(stream._sig);
	}

	template<Signal T> auto get()
	{
		return [&](auto&& in) -> T& {
			return ::gtry::scl::strm::get<T>(std::forward<decltype(in)>(in));
		};
	}

	template<Signal AddT, Signal PayloadT, Signal ...MetaT>
	auto attach(Stream<PayloadT, MetaT...>&& stream, AddT&& signal)
	{
		using Tplain = std::remove_reference_t<AddT>;

		if constexpr (Stream<PayloadT, MetaT...>::template has<Tplain>())
		{
			Stream<PayloadT, MetaT...> ret;
			connect(ret.data, stream.data);

			auto newMeta = std::apply([&](auto& ...meta) {
				auto fun = [&](auto& member) -> decltype(auto) {
					if constexpr (std::is_same_v<std::remove_cvref_t<decltype(member)>, Tplain>)
						return std::forward<decltype(member)>(signal);
					else
						return std::forward<decltype(member)>(member);
					};
				return std::tie(fun(meta)...);
				}, stream._sig);

			downstream(ret._sig) = downstream(newMeta);
			upstream(newMeta) = upstream(ret._sig);
			return ret;
		}
		else
		{
			if constexpr (std::is_same_v<Tplain, Ready>)
			{
				// Ready is always the first meta signal if present to fit R*Stream type declarations
				// This is a bit of a hack, all meta signals should have some kind of deterministic ordering instead
				Stream<PayloadT, Ready, MetaT...> ret;
				connect(*ret, stream.data);
				get<Ready>(ret) <<= signal;

				std::apply([&](auto& ...meta) {
					((get<std::remove_reference_t<decltype(meta)>>(ret) <<= meta), ...);
					}, stream._sig);
				return ret;
			}
			else
			{
				Stream<PayloadT, MetaT..., Tplain> ret;
				connect(*ret, stream.data);
				get<Tplain>(ret) <<= signal;

				std::apply([&](auto& ...meta) {
					((get<std::remove_reference_t<decltype(meta)>>(ret) <<= meta), ...);
					}, stream._sig);
				return ret;
			}
		}
	}

	template<Signal AddT, Signal PayloadT, Signal ...MetaT>
	auto attach(const Stream<PayloadT, MetaT...>& stream, AddT&& signal)
	{
		using Tplain = std::remove_reference_t<AddT>;

		if constexpr (Stream<PayloadT, MetaT...>::template has<Tplain>())
		{
			Stream<PayloadT, MetaT...> ret = stream;
			ret.set(std::forward<AddT>(signal));
			return ret;
		}
		else if constexpr (std::is_same_v<Tplain, Ready>)
		{
			Stream<PayloadT, Ready, MetaT...> ret{ stream.data };
			get<Ready>(ret) <<= signal;

			std::apply([&](auto& ...meta) {
				((get<std::remove_cvref_t<decltype(meta)>>(ret) = meta), ...);
			}, stream._sig);
			return ret;
		}
		else
		{
			return Stream<PayloadT, MetaT..., Tplain>{
				stream.data,
					std::apply([&](auto&... element) {
						return std::tuple(element..., std::forward<AddT>(signal));
					}, stream._sig)
			};
		}
	}

	// Forward declared here, actually in utils.h
	template<StreamSignal BeatStreamT, StreamSignal PacketStreamT>
	std::tuple<PacketStreamT, BeatStreamT> replicateForEntirePacket(PacketStreamT&& packetStream, BeatStreamT&& beatStream);

	template<Signal AddPayloadT, Signal ...AddMetaT, Signal PayloadT, Signal ...Meta> 
	auto attach(Stream<PayloadT, Meta...>&& stream, Stream<AddPayloadT, AddMetaT...>&& metaStream) 
	{
		auto [result, duplicated] = replicateForEntirePacket(move(stream), move(metaStream));
		ready(duplicated) = '1';

		Stream<PayloadT, Meta..., AddPayloadT> out = strm::attach(move(result), move(*duplicated));

		if constexpr (Stream<PayloadT, Meta...>::template has<Error>())
			error(out) |= error(duplicated);

		return out;
	}

	template<Signal AddT>
	inline auto attach(AddT&& signal)
	{
		return [&](auto&& in) {
			return ::gtry::scl::strm::attach(std::forward<decltype(in)>(in), std::forward<AddT>(signal));
		};
	}

	template<Signal Wrapper, StreamSignal T, Signal PayloadT, Signal ...MetaT>
	auto attachAs(Stream<PayloadT, MetaT...>&& stream, T&& metaStream) 
	{
		return attach(move(stream), transform(std::forward<T>(metaStream), [](const auto& v) { return Wrapper{ v }; }));
	}

	template<Signal Wrapper, StreamSignal T> auto attachAs(T&& metaStream)
	{
		return [&](auto&& in) {
			return ::gtry::scl::strm::attachAs<Wrapper>(std::forward<decltype(in)>(in), std::forward<T>(metaStream));
		};
	}

	template<StreamSignal StreamT, typename FunT>
	auto transform(StreamT&& stream, FunT&& fun)
	{
		return Stream{
			std::invoke(std::forward<FunT>(fun), std::forward<StreamT>(stream).data),
			connect(std::forward<StreamT>(stream)._sig)
		};
	}

	template<typename FunT>
	auto transform(FunT&& fun)
	{
		return [&](StreamSignal auto&& in) {
			return ::gtry::scl::strm::transform(std::forward<decltype(in)>(in), std::forward<FunT>(fun));
		};
	}

	namespace internal
	{
		template<typename QueryMeta, Signal PayloadT, Signal ...Meta>
		int reductionChecker(const Stream<PayloadT, Meta...>&) { 
			static_assert(Stream<PayloadT, Meta...>::template has<QueryMeta>(), "Trying to reduce to a stream type that actually has additional meta flags in its signature.");
			return 0;
		}
	}

	template<StreamSignal T, StreamSignal srcT>
	T reduceTo(srcT&& stream)
	{
		T ret{
			connect(std::forward<srcT>(stream).data)
		};

		std::apply([&](auto&... meta) {
			(internal::reductionChecker<std::remove_cvref_t<decltype(meta)>>(stream), ...);
			((meta <<= std::get<std::remove_cvref_t<decltype(meta)>>(std::forward<srcT>(stream)._sig)), ...);
		}, ret._sig);
		return ret;
	}

	template<StreamSignal SigT>
	auto reduceTo()
	{
		return [&](StreamSignal auto&& in) {
			return ::gtry::scl::strm::reduceTo<SigT>(std::forward<decltype(in)>(in));
		};
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T> 
	Stream<PayloadT, Meta...>::operator T ()
	{
		T ret{ data };

		auto assignIfExist = [&](auto&& ms) {
			if constexpr (T::template has<std::remove_cvref_t<decltype(ms)>>())
				get<std::remove_cvref_t<decltype(ms)>>(ret) <<= ms;
		};

		std::apply([&](auto&... meta) {
			(assignIfExist(meta), ...);
		}, _sig);
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T> 
	Stream<PayloadT, Meta...>::operator T () const requires(GTRY_STREAM_ASSIGNABLE)
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

	template<Signal T, StreamSignal StreamT>
	auto remove(StreamT&& stream)
	{
		auto metaRefs = internal::remove_from_tuple<T>(stream._sig);

		return std::apply([&](auto&... meta) {
			using PayloadT = std::remove_cvref_t<decltype(stream.data)>;

			Stream<PayloadT, std::remove_cvref_t<decltype(meta)>...> ret{
				connect(std::forward<StreamT>(stream).data),
			};
			downstream(ret._sig) = downstream(metaRefs);
			upstream(metaRefs) = upstream(ret._sig);
			return ret;
		}, metaRefs);
	}

	template<Signal SigT>
	auto remove()
	{
		return [&](StreamSignal auto&& in) {
			return ::gtry::scl::strm::remove<SigT>(std::forward<decltype(in)>(in));
		};
	}

	template<StreamSignal StreamT>
	auto removeUpstream(StreamT&& stream) { return ::gtry::scl::strm::remove<Ready>(std::forward<StreamT>(stream)); }

	inline auto removeUpstream()
	{
		return [&](StreamSignal auto&& in) {
			return ::gtry::scl::strm::removeUpstream(std::forward<decltype(in)>(in));
		};
	}

	template<StreamSignal StreamT>
	auto removeFlowControl(StreamT&& stream) { return std::forward<StreamT>(stream) | remove<Ready>() | remove<Valid>() | remove<Sop>(); }

	inline auto removeFlowControl()
	{
		return [&](StreamSignal auto&& in) {
			return ::gtry::scl::strm::removeFlowControl(std::forward<decltype(in)>(in));
		};
	}
}

namespace gtry {


	template<scl::strm::StreamSignal T>
	struct VisitCompound<T>
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (T& a, const T& b, Visitor& v)
		{
			if (v.enterPackStruct(a)) {
				std::apply([&](auto&... meta) {
					(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
						meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
						, ...);
				}, a._sig);

				VisitCompound<typename T::Payload>{}(a.data, b.data, v);
			} else
				v(a);
			v.leavePack();
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (T& a, Visitor& v)
		{
			if (v.enterPackStruct(a)) {
				std::apply([&](auto&... meta) {
					(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(meta, v), ...);
				}, a._sig);

				VisitCompound<typename T::Payload>{}(a.data, v);
			} else
				v(a);
			v.leavePack();
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const T& a, const T& b, Visitor& v)
		{
			if (v.enterPackStruct(a)) {
				std::apply([&](auto&... meta) {
					(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
						meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
						, ...);
				}, a._sig);

				VisitCompound<typename T::Payload>{}(a.data, b.data, v);
			} else
				v(a);
			v.leavePack();
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


