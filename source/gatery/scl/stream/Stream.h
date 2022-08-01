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
#include "../Fifo.h"
#include "../flag.h"

namespace gtry::scl
{
	template<Signal PayloadT, Signal... Meta>
	struct Stream;

	struct Ready;
	struct Valid;
	struct ByteEnable;
	struct Error;

	namespace internal
	{
		template<class T>
		struct is_stream_signal : std::false_type {};

		template<Signal... T>
		struct is_stream_signal<Stream<T...>> : std::true_type {};
	}

	template<class T>
	concept StreamSignal = Signal<T> and internal::is_stream_signal<T>::value;

	template<class T>
	concept BidirStreamSignal = StreamSignal<T> and !requires(T& a, const T& b) { a = b; };

	template<Signal PayloadT, Signal... Meta>
	struct Stream
	{
		using Payload = PayloadT;

		Payload data;
		std::tuple<Meta...> _sig;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		Payload* operator ->() { return &data; }
		const Payload* operator ->() const { return &data; }

		template<Signal T>
		static constexpr bool has();

		template<Signal T> auto add(T&& signal);
		template<Signal T> auto add(T&& signal) const requires (!BidirStreamSignal<Stream<PayloadT, Meta...>>);

		template<Signal T> constexpr T& get() { return std::get<T>(_sig); }
		template<Signal T> constexpr const T& get() const { return std::get<T>(_sig); }
		template<Signal T> constexpr void set(T&& signal) { get<T>() = std::forward<T>(signal); }

		auto transform(std::invocable<Payload> auto&& fun);
		auto transform(std::invocable<Payload> auto&& fun) const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>);

		template<StreamSignal T> T reduceTo();
		template<StreamSignal T> T reduceTo() const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>);

		template<Signal T> auto remove();
		template<Signal T> auto remove() const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>);

		/**
		 * @brief	Puts a register in the valid and data path.
					The register enable is connected to ready and ready is just forwarded.
					Note that valid will not become high while ready is low, which violates stream semantics.
		 * @param	Settings forwarded to all instantiated registers.
		 * @return	connected stream
		*/
		Stream regDownstreamBlocking(const RegisterSettings& settings = {});

		/**
		 * @brief	Puts a register in the valid and data path.
					This version ensures that data is captured when ready is low to fill pipeline bubbles.
		 * @param	Settings forwarded to all instantiated registers.
		 * @return	connected stream
		*/
		Stream regDownstream(const RegisterSettings& settings = {});

		/**
		 * @brief	Puts a register in the ready path.
		 * @param	Settings forwarded to all instantiated registers.
		 * @return	connected stream
		*/
		Stream regReady(const RegisterSettings& settings = {});

		/**
		 * @brief Create a FIFO for buffering.
		 * @param minDepth The FIFO can hold at least that many data beats. 
							The actual amount depends on the available target architecture. 
		 * @return connected stream
		*/
		Stream fifo(size_t minDepth = 16);

		/**
		 * @brief Attach the stream as source and a new stream as sink to the FIFO.
		 *			This is useful to make further settings or access advanced FIFO signals.
		 *			For clock domain crossing you should use gtry::connect.
		 * @param instance The FIFO to use.
		 * @return connected stream
		*/
		template<Signal T>
		Stream fifo(Fifo<T>& instance);
	};

	/**
	 * @brief High when all transfer conditions (ready and valid high) are met.
	 * @param stream to test
	 * @return transfer occurring
	*/
	template<StreamSignal T> Bit transfer(const T& stream) { return valid(stream) & ready(stream); }

	/**
	 * @brief High when sink can accept incoming data.
	 * @param stream to test
	 * @return sink ready
	*/
	template<StreamSignal T> const Bit ready(const T& stream) { return '1'; }

	/**
	 * @brief High when source has data to send.
	 * @param stream to test
	 * @return source has data
	*/
	template<StreamSignal T> const Bit valid(const T& stream) { return '1'; }

	template<StreamSignal T> const Bit eop(const T& stream) { return '1'; }
	template<StreamSignal T> const Bit sop(const T& stream) { return '1'; }
	template<StreamSignal T> const UInt byteEnable(const T& stream) { return ConstUInt(1, 1_b); }
	template<StreamSignal T> const Bit error(const T& stream) { return '0'; }

	struct Ready
	{
		Reverse<Bit> ready;
	};

	template<StreamSignal T> requires (T::template has<Ready>())
	Bit& ready(T& stream) { return *stream.template get<Ready>().ready; }
	template<StreamSignal T> requires (T::template has<Ready>())
	const Bit& ready(const T& stream) { return *stream.template get<Ready>().ready; }


	struct Valid
	{
		// reset to zero
		Bit valid = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal T> requires (T::template has<Valid>())
	Bit& valid(T& stream) { return stream.template get<Valid>().valid; }
	template<StreamSignal T> requires (T::template has<Valid>())
	const Bit& valid(const T& stream) { return stream.template get<Valid>().valid; }


	struct ByteEnable
	{
		BVec byteEnable;
	};
	template<StreamSignal T> requires (T::template has<ByteEnable>())
	BVec& byteEnable(T& stream) { return stream.template get<ByteEnable>().byteEnable; }
	template<StreamSignal T> requires (T::template has<ByteEnable>())
	const BVec& byteEnable(const T& stream) { return stream.template get<ByteEnable>().byteEnable; }


	struct Error
	{
		Bit error;
	};

	template<StreamSignal T> requires (T::template has<Error>())
	Bit& error(T& stream) { return stream.template get<Error>().error; }
	template<StreamSignal T> requires (T::template has<Error>())
	const Bit& error(const T& stream) { return stream.template get<Error>().error; }


	template<Signal T, Signal... Meta>
	using RvStream = Stream<T, scl::Ready, scl::Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using VStream = Stream<T, scl::Valid, Meta...>;

	/**
	 * @brief Puts a register in the ready, valid and data path.
	 * @param stream Source stream
	 * @param Settings forwarded to all instantiated registers.
	 * @return connected stream
	*/
	template<StreamSignal T> 
	T reg(T& stream, const RegisterSettings& settings = {});
	template<StreamSignal T>
	T reg(const T& stream, const RegisterSettings& settings = {});

	/**
	 * @brief Connect a Stream as source to a FIFO as sink.
	 * @param sink FIFO instance.
	 * @param source Stream instance.
	*/
	template<Signal Tf, StreamSignal Ts>
	void connect(Fifo<Tf>& sink, Ts& source);

	/**
	 * @brief Connect a FIFO as source to a Stream as sink.
	 * @param sink Stream instance.
	 * @param source FIFO instance.
	*/
	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, Fifo<Tf>& source);
}

namespace gtry::scl
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
			*ret <<= data;
			ret.template get<T>() <<= signal;

			std::apply([&](auto& ...meta) {
				((ret.template get<std::remove_reference_t<decltype(meta)>>() <<= meta), ...);
			}, _sig);
			return ret;
		}
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::add(T&& signal) const requires (!BidirStreamSignal<Stream<PayloadT, Meta...>>)
	{
		if constexpr (has<T>())
		{
			Stream<PayloadT, Meta...> ret = *this;
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
		auto&& result = std::invoke(fun, data);
		Stream<std::remove_cvref_t<decltype(result)>, Meta...> ret;
		ret.data <<= result;
		ret._sig <<= _sig;
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	inline auto Stream<PayloadT, Meta...>::transform(std::invocable<Payload> auto&& fun) const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>)
	{
		auto&& result = std::invoke(fun, data);
		Stream<std::remove_cvref_t<decltype(result)>, Meta...> ret;
		ret.data = result;
		ret._sig = _sig;
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T>
	inline T Stream<PayloadT, Meta...>::reduceTo()
	{
		T ret;
		ret.data <<= data;

		std::apply([&](auto&... meta) {
			((meta <<= std::get<std::remove_cvref_t<decltype(meta)>>(_sig)), ...);
		}, ret._sig);
		return ret;
	}

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T>
	inline T Stream<PayloadT, Meta...>::reduceTo() const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>)
	{
		T ret{ data };
		std::apply([&](auto&... meta) {
			((ret.template get<std::remove_cvref_t<decltype(meta)>>() = meta), ...);
		}, ret._sig);
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
			ret.data <<= this->data;
			downstream(ret._sig) = downstream(metaRefs);
			upstream(metaRefs) = upstream(ret._sig);
			return ret;
		}, metaRefs);
	}

	template<Signal PayloadT, Signal ...Meta>
	template<Signal T>
	inline auto Stream<PayloadT, Meta...>::remove() const requires(!BidirStreamSignal<Stream<PayloadT, Meta...>>)
	{
		auto metaRefs = internal::remove_from_tuple<T>(_sig);

		return std::apply([&](auto&... meta) {
			return Stream<PayloadT, std::remove_cvref_t<decltype(meta)>...>{
				this->data, metaRefs
			};
		}, metaRefs);
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> gtry::scl::Stream<PayloadT, Meta...>::regDownstreamBlocking(const RegisterSettings& settings)
	{
		auto dsSig = constructFrom(copy(downstream(*this)));

		IF(ready(*this))
			dsSig = downstream(*this);

		dsSig = reg(dsSig, settings);

		Stream<PayloadT, Meta...> ret;
		downstream(ret) = dsSig;
		upstream(*this) = upstream(ret);
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::regDownstream(const RegisterSettings& settings)
	{
		Stream<PayloadT, Meta...> ret;

		if constexpr (has<Ready>())
		{
			Bit valid_reg;
			auto dsSig = constructFrom(copy(downstream(*this)));

			IF(!valid_reg | ready(*this))
			{
				valid_reg = valid(*this);
				dsSig = downstream(*this);
			}

			valid_reg = reg(valid_reg, '0', settings);
			dsSig = reg(dsSig, settings);

			downstream(ret) = dsSig;
			upstream(*this) = upstream(ret);
			ready(*this) |= !valid_reg;
		}
		else
		{
			downstream(ret) = reg(copy(downstream(*this)));
			upstream(*this) = upstream(ret);
		}
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::regReady(const RegisterSettings& settings)
	{
		Stream<PayloadT, Meta...> ret;
		ret <<= *this;

		if constexpr (has<Ready>())
		{
			Bit valid_reg;
			auto data_reg = constructFrom(copy(downstream(*this)));

			// we are ready as long as our buffer is unused
			ready(*this) = !valid_reg;

			IF(ready(ret))
				valid_reg = '0';

			IF(!valid_reg)
			{
				IF(!ready(ret))
					valid_reg = valid(*this);
				data_reg = downstream(*this);
			}

			valid_reg = reg(valid_reg, '0', settings);
			data_reg = reg(data_reg, settings);

			IF(valid_reg)
			{
				valid(ret) = '1';
				downstream(ret) = data_reg;
			}
		}
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::fifo(size_t minDepth)
	{
		Fifo inst{ minDepth, copy(downstream(*this)) };
		Stream ret = fifo(inst);
		inst.generate();
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	template<Signal T>
	inline Stream<PayloadT, Meta...> gtry::scl::Stream<PayloadT, Meta...>::fifo(Fifo<T>& instance)
	{
		connect(instance, *this);

		Stream<PayloadT, Meta...> ret;
		connect(ret, instance);
		return ret;
	}

	template<Signal Tf, StreamSignal Ts>
	void connect(Fifo<Tf>& sink, Ts& source)
	{
		IF(transfer(source))
			sink.push(downstream(source));
		ready(source) = !sink.full();
	}

	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, Fifo<Tf>& source)
	{
		downstream(sink) = source.peek();
		valid(sink) = !source.empty();
	
		IF(transfer(sink))
			source.pop();
	}

	template<StreamSignal T>
	T reg(T& stream, const RegisterSettings& settings)
	{
		// we can use blocking reg here since regReady guarantees high ready signal
		return stream.regDownstreamBlocking(settings).regReady(settings);
	}

	template<StreamSignal T>
	T reg(const T& stream, const RegisterSettings& settings)
	{
		static_assert(!stream.template has<Ready>(), "cannot create upstream register from const stream");
		return stream.regDownstream(settings);
	}
}

namespace gtry {

	template<scl::StreamSignal T>
	struct VisitCompound<T>
	{
		void operator () (T& a, const T& b, CompoundVisitor& v, size_t flags)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
					meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
					, ...);
			}, a._sig);

			v.enter("data");
			VisitCompound<typename T::Payload>{}(a.data, b.data, v, flags);
			v.leave();
		}

		void operator () (T& a, CompoundVisitor& v)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(meta, v), ...);
			}, a._sig);

			v.enter("data");
			VisitCompound<typename T::Payload>{}(a.data, v);
			v.leave();
		}

		void operator () (const T& a, const T& b, CompoundVisitor& v)
		{
			std::apply([&](auto&... meta) {
				(VisitCompound<std::remove_reference_t<decltype(meta)>>{}(
					meta, b.template get<std::remove_reference_t<decltype(meta)>>(), v)
					, ...);
			}, a._sig);

			v.enter("data");
			VisitCompound<typename T::Payload>{}(a.data, b.data, v);
			v.leave();
		}
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::Ready, ready);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Valid, valid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::ByteEnable, byteEnable);
