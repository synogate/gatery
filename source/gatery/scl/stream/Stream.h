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
	struct Ready;
	struct Valid;
	struct Eop;
	struct Sop;
	struct ByteEnable;

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
		static constexpr bool has() { 
			return 
				std::is_base_of_v<std::remove_reference_t<T>, std::remove_reference_t<PayloadT>> |
				(std::is_same_v<std::remove_reference_t<Meta>, std::remove_reference_t<T>> | ...);
		}

		template<Signal T> constexpr T& get() { return std::get<T>(_sig); }
		template<Signal T> constexpr const T& get() const { return std::get<T>(_sig); }
		template<Signal T> constexpr void set(T&& signal) { get<T>() = std::forward<T>(signal); }

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

	namespace internal
	{
		template<class T>
		struct is_stream_signal : std::false_type {};

		template<Signal... T>
		struct is_stream_signal<Stream<T...>> : std::true_type {};
	}

	template<class T>
	concept StreamSignal = Signal<T> and internal::is_stream_signal<T>::value;

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
	template<StreamSignal T> const UInt byteEnable(const T& stream) { return ConstBVec(1, 1_b); }

	struct Ready
	{
		Reverse<Bit> ready;
	};

	template<StreamSignal T> requires (T::template has<Ready>())
	Bit& ready(T& stream) { return *stream.get<Ready>().ready; }
	template<StreamSignal T> requires (T::template has<Ready>())
	const Bit& ready(const T& stream) { return *stream.get<Ready>().ready; }


	struct Valid
	{
		// reset to zero
		Bit valid = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal T> requires (T::template has<Valid>())
	Bit& valid(T& stream) { return stream.get<Valid>().valid; }
	template<StreamSignal T> requires (T::template has<Valid>())
	const Bit& valid(const T& stream) { return stream.get<Valid>().valid; }


	struct Eop
	{
		Bit eop;
	};

	template<StreamSignal T> requires (T::template has<Eop>())
	Bit& eop(T& stream) { return stream.get<Eop>().eop; }
	template<StreamSignal T> requires (T::template has<Eop>())
	const Bit& eop(const T& stream) { return stream.get<Eop>().eop; }
	template<StreamSignal T> requires (T::template has<Valid>() and T::template has<Eop>())
	const Bit sop(const T& signal) { return !flag(transfer(signal), eop(signal)); }


	struct Sop
	{
		// reset to zero, sop is used for packet streams without valid.
		Bit sop = Bit{ SignalReadPort{}, false };
	};

	template<StreamSignal T> requires (T::template has<Sop>())
	Bit& sop(T& stream) { return stream.get<Sop>().sop; }
	template<StreamSignal T> requires (T::template has<Sop>())
	const Bit& sop(const T& stream) { return stream.get<Sop>().sop; }
	template<StreamSignal T> requires (!T::template has<Valid>() and T::template has<Sop>() and T::template has<Eop>())
	const Bit valid(const T& signal) { return flag(sop(signal), eop(signal)) | sop(signal); }


	struct ByteEnable
	{
		BVec byteEnable;
	};
	template<StreamSignal T> requires (T::template has<ByteEnable>())
	BVec& byteEnable(T& stream) { return stream.get<ByteEnable>().byteEnable; }
	template<StreamSignal T> requires (T::template has<ByteEnable>())
	const BVec& byteEnable(const T& stream) { return stream.get<ByteEnable>().byteEnable; }



	template<Signal T, Signal... Meta>
	using RvStream = Stream<T, scl::Ready, scl::Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using VStream = Stream<T, scl::Valid, Meta...>;

	template<Signal T, Signal... Meta>
	using RvPacketStream = Stream<T, scl::Ready, scl::Valid, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using VPacketStream = Stream<T, scl::Valid, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using RsPacketStream = Stream<T, scl::Ready, scl::Valid, scl::Eop, Meta...>;

	template<Signal T, Signal... Meta>
	using SPacketStream = Stream<T, scl::Valid, scl::Eop, Meta...>;

	template<StreamSignal T>
	T makeStream(typename T::Payload&& data) { }


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
		downstream(sink) = source.peak();
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
		static_assert(!stream.has<Ready>(), "cannot create upstream register from const stream");
		return stream.regDownstream(settings);
	}
}
