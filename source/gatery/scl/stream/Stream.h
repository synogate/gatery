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
	struct TxId;
	struct Sop;

	namespace internal
	{
		template<class T>
		struct is_stream_signal : std::false_type {};

		template<Signal... T>
		struct is_stream_signal<Stream<T...>> : std::true_type {};
	}

#ifdef __clang__
	template<class T>
	concept StreamSignal = /*Signal<T> and */internal::is_stream_signal<T>::value;
#else
	template<class T>
	concept StreamSignal = Signal<T> and internal::is_stream_signal<T>::value;
#endif	

#ifdef __clang__
	template<typename T>
	concept Assignable = std::is_assignable_v<T,T>;
#else
	template<typename T>
	concept Assignable = requires(T& a, const T& b) { a = b; };
#endif

	template<class T>
	concept BidirStreamSignal = StreamSignal<T> and !Assignable<T>;

	template<Signal PayloadT, Signal... Meta>
	struct StreamAssignabilityTestHelper
	{
		PayloadT data;
		std::tuple<Meta...> _sig;
	};

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

		template<StreamSignal T> operator T ();
		template<StreamSignal T> operator T () const requires(Assignable<AssignabilityTestType>);

		template<Signal T> auto remove();
		template<Signal T> auto remove() const requires(Assignable<AssignabilityTestType>);
		auto removeUpstream() { return remove<Ready>(); }
		auto removeUpstream() const requires(Assignable<AssignabilityTestType>) { return *this; }
		auto removeFlowControl() { return remove<Ready>().template remove<Valid>().template remove<Sop>(); }
		auto removeFlowControl() const requires(Assignable<AssignabilityTestType>) { return remove<Valid>().template remove<Sop>(); }

		/**
		 * @brief	Puts a register in the valid and data path.
					The register enable is connected to ready and ready is just forwarded.
					Note that valid will not become high while ready is low, which violates stream semantics.
		 * @param	Settings forwarded to all instantiated registers.
		 * @return	connected stream
		*/
		Stream regDownstreamBlocking(const RegisterSettings& settings = {});
		Stream regDownstreamBlocking(const RegisterSettings& settings = {}) const requires(Assignable<std::tuple<PayloadT, Meta...>>);

		/**
		 * @brief	Puts a register in the valid and data path.
					This version ensures that data is captured when ready is low to fill pipeline bubbles.
		 * @param	Settings forwarded to all instantiated registers.
		 * @return	connected stream
		*/
		Stream regDownstream(const RegisterSettings& settings = {});
		Stream regDownstream(const RegisterSettings& settings = {}) const requires(Assignable<std::tuple<PayloadT, Meta...>>);

		/**
		 * @brief	Puts a register spawner for retiming in the valid and data path.
		 * @return	connected stream
		*/
		Stream pipeinputDownstream(PipeBalanceGroup& group);

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
	const Bit valid(const StreamSignal auto& stream) { return '1'; }
	const Bit eop(const StreamSignal auto& stream) { return '1'; }
	const Bit sop(const StreamSignal auto& stream) { return '1'; }
	const UInt byteEnable(const StreamSignal auto& stream) { return ConstUInt(1, 1_b); }
	const Bit error(const StreamSignal auto& stream) { return '0'; }
	const UInt txid(const StreamSignal auto& stream) { return 0; }

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


	struct TxId
	{
		UInt txid;
	};

	template<StreamSignal T> requires (T::template has<TxId>())
	UInt& txid(T& stream) { return stream.template get<TxId>().txid; }
	template<StreamSignal T> requires (T::template has<TxId>())
	const UInt& txid(const T& stream) { return stream.template get<TxId>().txid; }

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

	template<Signal T>
	void connect(Fifo<T>& sink, RvStream<T>& source);

	/**
	 * @brief Connect a FIFO as source to a Stream as sink.
	 * @param sink Stream instance.
	 * @param source FIFO instance.
	*/
	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, Fifo<Tf>& source);

	template<Signal T>
	void connect(RvStream<T>& sink, Fifo<T>& source);


	template<StreamSignal T> 
	SimProcess performTransferWait(const T &stream, const Clock &clock) {
		co_await OnClk(clock);
	}

	template<StreamSignal T> requires (T::template has<Ready>() && !T::template has<Valid>())
	SimProcess performTransferWait(const T &stream, const Clock &clock) {
		do
			co_await OnClk(clock);
		while (!simu(ready(stream)));
	}

	template<StreamSignal T> requires (!T::template has<Ready>() && T::template has<Valid>())
	SimProcess performTransferWait(const T &stream, const Clock &clock) {
		do
			co_await OnClk(clock);
		while (!simu(valid(stream)));
	}

	template<StreamSignal T> requires (T::template has<Ready>() && T::template has<Valid>())
	SimProcess performTransferWait(const T &stream, const Clock &clock) {
		do
			co_await OnClk(clock);
		while (!simu(ready(stream)) || !simu(valid(stream)));
	}

	template<StreamSignal T>
	SimProcess performTransfer(const T& stream, const Clock& clock) 
	{
		co_await OnClk(clock);
	}

	template<StreamSignal T> requires (T::template has<Valid>())
	SimProcess performTransfer(const T& stream, const Clock& clock) 
	{
		simu(valid(stream)) = '1';
		co_await performTransferWait(stream, clock);
		simu(valid(stream)) = '0';
	}
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

	template<Signal PayloadT, Signal ...Meta>
	template<StreamSignal T>
	inline T Stream<PayloadT, Meta...>::reduceTo()
	{
		T ret;
		connect(ret.data, data);

		std::apply([&](auto&... meta) {
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

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> gtry::scl::Stream<PayloadT, Meta...>::regDownstreamBlocking(const RegisterSettings& settings)
	{
		if constexpr (has<Valid>())
			valid(*this).resetValue('0');

		auto dsSig = constructFrom(copy(downstream(*this)));

		IF(ready(*this))
			dsSig = downstream(*this);

		dsSig = reg(dsSig, settings);

		Self ret;
		downstream(ret) = dsSig;
		upstream(*this) = upstream(ret);
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> gtry::scl::Stream<PayloadT, Meta...>::regDownstreamBlocking(const RegisterSettings& settings) const requires(Assignable<std::tuple<PayloadT, Meta...>>)
	{
		if constexpr (has<Valid>())
		{
			auto tmp = *this;
			valid(tmp).resetValue('0');
			return {
				.data = reg(tmp.data, settings),
				._sig = reg(tmp._sig, settings)
			};
		}
		else
		{
			return {
				.data = reg(data, settings),
				._sig = reg(_sig, settings)
			};
		}
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::regDownstream(const RegisterSettings& settings)
	{
		if constexpr (has<Valid>())
			valid(*this).resetValue('0');

		Self ret;

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
	Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::regDownstream(const RegisterSettings& settings) const requires(Assignable<std::tuple<PayloadT, Meta...>>)
	{
		return regDownstreamBlocking(settings);
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> gtry::scl::Stream<PayloadT, Meta...>::pipeinputDownstream(PipeBalanceGroup& group)
	{
		if constexpr (has<Valid>())
			valid(*this).resetValue('0');

		Self ret;
		downstream(ret) = group(copy(downstream(*this)));
		upstream(*this) = upstream(ret);
		return ret;
	}

	template<Signal PayloadT, Signal... Meta>
	inline Stream<PayloadT, Meta...> Stream<PayloadT, Meta...>::regReady(const RegisterSettings& settings)
	{
		if constexpr (has<Valid>())
			valid(*this).resetValue('0');
		if constexpr (has<Ready>())
			ready(*this).resetValue('0');

		Self ret;
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
				downstream(ret) = data_reg;
				valid(ret) = '1';
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

		Self ret;
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

	template<Signal T>
	void connect(Fifo<T>& sink, RvStream<T>& source)
	{
		IF(transfer(source))
			sink.push(*source);
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

	template<Signal T>
	void connect(RvStream<T>& sink, Fifo<T>& source)
	{
		*sink = source.peek();
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

	template<Signal Tp, Signal... Meta>
	RvStream<Tp, Meta...> synchronizeStreamReqAck(RvStream<Tp, Meta...>& in, const Clock& inClock, const Clock& outClock)
	{
		Area area("synchronizeStreamReqAck", true);
		ClockScope csIn{ inClock };
		Stream crossingStream = in
			.template remove<Ready>()
			.template remove<Valid>();

		Bit eventIn;
		Bit idle = flag(ready(in), eventIn, '1');
		eventIn = valid(in) & idle;
		HCL_NAMED(eventIn);	

		Bit outputEnableCondition = synchronizeEvent(eventIn, inClock, outClock);
		HCL_NAMED(outputEnableCondition);

		crossingStream = reg(crossingStream);

		ClockScope csOut{ outClock };
		
		crossingStream = allowClockDomainCrossing(crossingStream, inClock, outClock);

		ENIF(outputEnableCondition){
			Clock dontSimplifyEnableRegClk = outClock.deriveClock({ .synchronizationRegister = true });
			crossingStream = reg(crossingStream, RegisterSettings{ .clock = dontSimplifyEnableRegClk });
		}

		RvStream out = crossingStream
			.add(Ready{})
			.add(Valid{})
			.template reduceTo<RvStream<Tp, Meta...>>();

		Bit outValid;
		outValid = flag(outputEnableCondition, outValid & ready(out));
		valid(out) = outValid;

		ready(in) = synchronizeEvent(transfer(out), outClock, inClock);
		
		return out;
	}

	template<StreamSignal T>
	T pipeinput(T& stream, PipeBalanceGroup& group) 
	{
		return stream.pipeinputDownstream(group);
	}

	template<Signal T, Signal... Meta>
	Stream<T, Meta...> lookup(Stream<UInt, Meta...>& addr, Memory<T>& memory)
	{
		Stream<T, Meta...> out = addr.transform([&](const UInt& address) {
			return memory[address].read();
		});
		if(memory.readLatencyHint())
			return out.regDownstream(RegisterSettings{ .allowRetimingBackward = true });
		return out;
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

BOOST_HANA_ADAPT_STRUCT(gtry::scl::Ready, ready);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::Valid, valid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::ByteEnable, byteEnable);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::TxId, txid);
