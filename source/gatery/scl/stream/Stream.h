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


namespace gtry::scl::strm
{
	using std::move;

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
	concept StreamSignal = /*Signal<T> and */internal::is_stream_signal<std::remove_cvref_t<T>>::value;
#else
	template<class T>
	concept StreamSignal = Signal<T> and internal::is_stream_signal<std::remove_cvref_t<T>>::value;
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
		static constexpr bool has(); //not standalone? not sure, yes

		template<Signal T> inline auto add(T&& signal); //standalone
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
	};
	
	template<Signal T, StreamSignal StreamT> 
	auto remove(StreamT&& in);
	
	template<StreamSignal StreamT>
	inline auto removeFlowControl(StreamT&& in) { return remove<Ready>(remove<Valid>(remove<Sop>(move(in)))); }

	template<StreamSignal StreamT>
	inline auto removeUpstream(StreamT&& in) { return remove<Ready>(move(in)); }

	/**
	* @brief	Puts a register spawner for retiming in the valid and data path.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group);

	//untested
	auto pipeinputDownstream(PipeBalanceGroup& group)
	{
		return [=](auto&& in) { return pipeinputDownstream(std::forward<decltype(in)>(in), group); };
	}

	/**
	* @brief	Puts a register in the valid and data path.
	This version ensures that data is captured when ready is low to fill pipeline bubbles.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regDownstream(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	auto regDownstream(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDownstream(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief	Puts a register in the ready path and adds additional circuitry to keep the expected behavior of a stream
	* @param	in The input stream.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regReady(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	auto regReady(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regReady(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief	Puts a register in the valid and data path.
	The register enable is connected to ready and ready is just forwarded.
	Note that valid will not become high while ready is low, which violates stream semantics.
	* @param	Settings forwarded to all instantiated registers.
	* @return	connected stream
	*/
	template<StreamSignal StreamT>
	StreamT regDownstreamBlocking(StreamT&& in, const RegisterSettings& settings = {});

	//untested
	auto regDownstreamBlocking(const RegisterSettings& settings = {})
	{
		return [=](auto&& in) { return regDownstreamBlocking(std::forward<decltype(in)>(in), settings); };
	}

	/**
	* @brief Attach the stream as source and a new stream as sink to the FIFO.
	*			This is useful to make further settings or access advanced FIFO signals.
	*			For clock domain crossing you should use gtry::connect.
	* @param in The input stream.
	* @param instance The FIFO to use.
	* @param fallThrough allow data to flow past the fifo in the same cycle when it's empty.
	* @return connected stream
	*/
	template<Signal T, StreamSignal StreamT>
	StreamT fifo(StreamT&& in, Fifo<T>& instance, FallThrough fallThrough = FallThrough::off);

	/**
	* @brief Create a FIFO for buffering.
	* @param in The input stream.
	* @param minDepth The FIFO can hold at least that many data beats.
	The actual amount depends on the available target architecture.
	* @param fallThrough allow data to flow past the fifo in the same cycle when it's empty.
	* @return connected stream
	*/
	template<StreamSignal StreamT>
	StreamT fifo(StreamT&& in, size_t minDepth = 16, FallThrough fallThrough = FallThrough::off);

	auto fifo(size_t minDepth = 16, FallThrough fallThrough = FallThrough::off)
	{
		return [=](auto&& in) { return fifo(std::forward<decltype(in)>(in), minDepth, fallThrough); };
	}

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
	using RvStream = Stream<T, Ready, Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using VStream = Stream<T, Valid, Meta...>;


	template<StreamSignal T> 
	auto simuReady(const T &stream) {
		if constexpr (stream.template has<Ready>())
			return simu(ready(stream));
		else
			return '1';
	}

	template<StreamSignal T> 
	auto simuValid(const T &stream) {
		if constexpr (stream.template has<Valid>())
			return simu(valid(stream));
		else
			return '1';
	}

	/**
	 * @brief Puts a register in the ready, valid and data path.
	 * @param stream Source stream
	 * @param Settings forwarded to all instantiated registers.
	 * @return connected stream
	*/
	template<StreamSignal T> 
	T regDecouple(T& stream, const RegisterSettings& settings = {});
	template<StreamSignal T>
	T regDecouple(const T& stream, const RegisterSettings& settings = {});

	using gtry::connect;
	/**
	 * @brief Connect a Stream as source to a FIFO as sink.
	 * @param sink FIFO instance.
	 * @param source Stream instance.
	*/
	template<Signal Tf, StreamSignal Ts>
	void connect(scl::Fifo<Tf>& sink, Ts& source);

	template<Signal T>
	void connect(scl::Fifo<T>& sink, RvStream<T>& source);

	/**
	 * @brief Connect a FIFO as source to a Stream as sink.
	 * @param sink Stream instance.
	 * @param source FIFO instance.
	*/
	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, scl::Fifo<Tf>& source);

	template<Signal T>
	void connect(RvStream<T>& sink, scl::Fifo<T>& source);


	using gtry::pipeinput;
	/// Add register spawners to the downstream signals which are enabled by the upstream ready (if it exists).
	template<StreamSignal T>
	T pipeinput(T&& in)
	{
		T out;
		ENIF(ready(out))
		{
			PipeBalanceGroup group;
			if constexpr (T::template has<Valid>())
				valid(in).resetValue('0');

			downstream(out) = gtry::pipeinput(copy(downstream(in)), group);
		}
		upstream(in) = upstream(out);
		return out;
	}
}


namespace gtry::scl {

	using std::move;

	using strm::Stream;
	using strm::VStream;
	using strm::RvStream;

	using strm::Ready;
	using strm::Valid;
	using strm::ByteEnable;
	using strm::Error;
	using strm::TxId;
	using strm::Sop;

	using strm::StreamSignal;


	namespace internal
	{
		template<StreamSignal T>
		SimProcess performTransferWait(const T& stream, const Clock& clock) {
			co_await OnClk(clock);
		}

		template<StreamSignal T> requires (T::template has<Ready>() && !T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(ready(stream)));
		}

		template<StreamSignal T> requires (!T::template has<Ready>() && T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(valid(stream)));
		}

		template<StreamSignal T> requires (T::template has<Ready>() && T::template has<Valid>())
			SimProcess performTransferWait(const T& stream, const Clock& clock) {
			do
				co_await OnClk(clock);
			while (!simu(ready(stream)) || !simu(valid(stream)));
		}
	}
	SimProcess performTransferWait(const StreamSignal auto& stream, const Clock& clock) { return internal::performTransferWait(stream, clock); }

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

	template<Signal T, StreamSignal StreamT>
	auto remove(StreamT&& in)
	{
		auto metaRefs = internal::remove_from_tuple<T>(in._sig);

		return std::apply([&](auto&... meta) {
			if constexpr (std::is_const_v<std::remove_reference_t<StreamT>>)
			{
				return Stream<std::remove_cvref_t<StreamT>::Payload, std::remove_cvref_t<decltype(meta)>...>{
					in.data, metaRefs
				};
			}
			else
			{
				Stream<std::remove_cvref_t<StreamT>::Payload, std::remove_cvref_t<decltype(meta)>...> ret;
				connect(ret.data, in.data);
				downstream(ret._sig) = downstream(metaRefs);
				upstream(metaRefs) = upstream(ret._sig);
				return ret;
			}
		}, metaRefs);
	}

	template<StreamSignal StreamT>
	StreamT strm::regDownstreamBlocking(StreamT&& in, const RegisterSettings& settings)
	{
		if constexpr (in.has<Valid>())
			valid(in).resetValue('0');

		auto dsSig = constructFrom(copy(downstream(in)));

		IF(ready(in))
			dsSig = downstream(in);

		dsSig = reg(dsSig, settings);

		StreamT ret;
		downstream(ret) = dsSig;
		upstream(in) = upstream(ret);
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT strm::regDownstream(StreamT &&in, const RegisterSettings& settings)
	{
		if constexpr (in.has<Valid>())
			valid(in).resetValue('0');

		StreamT ret;

		if constexpr (in.has<Ready>())
		{
			Bit valid_reg;
			auto dsSig = constructFrom(copy(downstream(in)));

			IF(!valid_reg | ready(in))
			{
				valid_reg = valid(in);
				dsSig = downstream(in);
			}

			valid_reg = reg(valid_reg, '0', settings);
			dsSig = reg(dsSig, settings);

			downstream(ret) = dsSig;
			upstream(in) = upstream(ret);
			ready(in) |= !valid_reg;
		}
		else
		{
			downstream(ret) = reg(copy(downstream(in)));
			upstream(in) = upstream(ret);
		}
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT strm::pipeinputDownstream(StreamT&& in, PipeBalanceGroup& group)
	{
		if constexpr (in.has<Valid>())
			valid(in).resetValue('0');

		StreamT ret;
		downstream(ret) = group(copy(downstream(in)));
		upstream(in) = upstream(ret);
		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT strm::regReady(StreamT &&in, const RegisterSettings& settings)
	{
		if constexpr (in.template has<Valid>())
			valid(in).resetValue('0');
		if constexpr (in.has<Ready>())
			ready(in).resetValue('0');

		Stream ret = constructFrom(in);
		ret <<= in;

		if constexpr (in.has<Ready>())
		{
			Bit valid_reg;
			auto data_reg = constructFrom(copy(downstream(in)));

			// we are ready as long as our buffer is unused
			ready(in) = !valid_reg;

			IF(ready(ret))
				valid_reg = '0';

			IF(!valid_reg)
			{
				IF(!ready(ret))
					valid_reg = valid(in);
				data_reg = downstream(in);
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



	template<Signal T, StreamSignal StreamT>
	StreamT strm::fifo(StreamT&& in, Fifo<T>& instance, FallThrough fallThrough)
	{
		StreamT ret;
		connect(ret, instance);

		if (fallThrough == FallThrough::on) 
		{
			IF(!valid(ret))
			{
				downstream(ret) = downstream(in);
				IF(ready(ret))
					valid(in) = '0';
			}
		}
		connect(instance, in);

		return ret;
	}

	template<StreamSignal StreamT>
	inline StreamT strm::fifo(StreamT&& in, size_t minDepth, FallThrough fallThrough)
	{
		Fifo inst{ minDepth, copy(downstream(in)) };
		Stream ret = fifo(move(in), inst, fallThrough);
		inst.generate();

		return ret;
	}

	template<Signal Tp, Signal... Meta>
	RvStream<Tp, Meta...> synchronizeStreamReqAck(RvStream<Tp, Meta...>& in, const Clock& inClock, const Clock& outClock)
	{
		Area area("synchronizeStreamReqAck", true);
		ClockScope csIn{ inClock };
		Stream crossingStream = remove<Ready>(remove<Valid>(move(in)));

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
		return scl::strm::pipeinputDownstream(move(stream), group);
	}

	template<Signal T, Signal... Meta>
	Stream<T, Meta...> lookup(Stream<UInt, Meta...>& addr, Memory<T>& memory)
	{
		Stream<T, Meta...> out = addr.transform([&](const UInt& address) {
			return memory[address].read();
		});
		if (memory.readLatencyHint())
		{
			for (size_t i = 0; i < memory.readLatencyHint() - 1; ++i)
				out <<= scl::strm::regDownstreamBlocking(move(out), { .allowRetimingBackward = true });
			return strm::regDownstream(move(out),{ .allowRetimingBackward = true });
		}
		return out;
	}

	template<Signal Tf, StreamSignal Ts>
	void connect(scl::Fifo<Tf>& sink, Ts& source)
	{
		IF(transfer(source))
			sink.push(downstream(source));
		ready(source) = !sink.full();
	}

	template<Signal T>
	void connect(scl::Fifo<T>& sink, RvStream<T>& source)
	{
		IF(transfer(source))
			sink.push(*source);
		ready(source) = !sink.full();
	}

	template<StreamSignal Ts, Signal Tf>
	void connect(Ts& sink, scl::Fifo<Tf>& source)
	{
		downstream(sink) = source.peek();
		valid(sink) = !source.empty();
	
		IF(transfer(sink))
			source.pop();
	}

	template<Signal T>
	void connect(RvStream<T>& sink, scl::Fifo<T>& source)
	{
		*sink = source.peek();
		valid(sink) = !source.empty();

		IF(transfer(sink))
			source.pop();
	}

	template<StreamSignal T>
	T regDecouple(T& stream, const RegisterSettings& settings)
	{
		// we can use blocking reg here since regReady guarantees high ready signal
		return strm::regReady(strm::regDownstreamBlocking(move(stream), settings), settings);
	}

	template<StreamSignal T>
	T regDecouple(const T& stream, const RegisterSettings& settings)
	{
		static_assert(!stream.template has<Ready>(), "cannot create upstream register from const stream");
		return strm::regDownstream(stream, settings);
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

BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Ready, ready);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Valid, valid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::ByteEnable, byteEnable);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::TxId, txid);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::strm::Error, error);
