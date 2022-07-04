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

namespace gtry::scl
{
	/**
	 * @brief Adds ready/valid handshake semantics to a signal.
	*/
	template<Signal Payload>
	struct Stream
	{
		// TODO: get rid of hana
		//Reverse<Bit> ready;
		//Bit valid;
		//Payload data;

		BOOST_HANA_DEFINE_STRUCT(Stream,
			(Reverse<Bit>, ready),
			(Bit, valid),
			(Payload, data)
		);

		/**
		 * @brief Accessor for data member to reduce syntax clutter.
		 * @return data
		*/
		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		/**
		 * @brief Accessor for recursive access to last data member.
		 *			Can be useful when chaining multiple templates to have a nice way to accessing 
		 *			the encapsulated Signal.
		 * @return final payload data
		*/
		decltype(auto) operator ->();
		decltype(auto) operator ->() const;

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
		Stream regUpstream(const RegisterSettings& settings = {});

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
		Stream fifo(Fifo<Payload>& instance);
	};

	/**
	 * @brief High when all transfer conditions (ready and valid high) are met.
	 * @param stream to test
	 * @return transfer occurring
	*/
	template<Signal T> Bit transfer(const Stream<T>& stream) { return stream.valid & *stream.ready; }

	/**
	 * @brief High when sink can accept incoming data.
	 * @param stream to test
	 * @return sink ready
	*/
	template<Signal T> const Bit& ready(const Stream<T>& stream) { return *stream.ready; }

	/**
	 * @brief High when source has data to send.
	 * @param stream to test
	 * @return source has data
	*/
	template<Signal T> const Bit& valid(const Stream<T>& stream) { return stream.valid; }

	/**
	 * @brief Puts a register in the ready, valid and data path.
	 * @param stream Source stream
	 * @param Settings forwarded to all instantiated registers.
	 * @return connected stream
	*/
	template<Signal T> Stream<T> reg(Stream<T>& stream, const RegisterSettings& settings = {});

	/**
	 * @brief Connect a Stream as source to a FIFO as sink.
	 * @param sink FIFO instance.
	 * @param source Stream instance.
	*/
	template<Signal T> void connect(Fifo<T>& sink, Stream<T>& source);

	/**
	 * @brief Connect a FIFO as source to a Stream as sink.
	 * @param sink Stream instance.
	 * @param source FIFO instance.
	*/
	template<Signal T> void connect(Stream<T>& sink, Fifo<T>& source);
}

namespace gtry::scl
{
	template<Signal Payload>
	decltype(auto) Stream<Payload>::operator ->()
	{
		if constexpr(requires(Payload & p) { p.operator->(); })
			return (Payload&)data;
		else
			return &data;
	}

	template<Signal Payload>
	decltype(auto) Stream<Payload>::operator ->() const
	{
		if constexpr(requires(Payload & p) { p.operator->(); })
			return (const Payload&)data;
		else
			return &data;
	}

	template<Signal Payload>
	inline Stream<Payload> gtry::scl::Stream<Payload>::regDownstreamBlocking(const RegisterSettings& settings)
	{
		Bit valid_reg;
		Payload data_reg = constructFrom(data);

		IF(*ready)
		{
			valid_reg = valid;
			data_reg = data;
		}

		valid_reg = reg(valid_reg, '0', settings);
		data_reg = reg(data_reg, settings);

		Stream<Payload> ret{
			.valid = valid_reg,
			.data = data_reg
		};
		*ready = *ret.ready;
		return ret;
	}

	template<Signal Payload>
	inline Stream<Payload> Stream<Payload>::regDownstream(const RegisterSettings& settings)
	{
		Bit valid_reg;
		Payload data_reg = constructFrom(data);

		IF(!valid_reg | *ready)
		{
			valid_reg = valid;
			data_reg = data;
		}

		valid_reg = reg(valid_reg, '0', settings);
		data_reg = reg(data_reg, settings);

		Stream<Payload> ret{
			.valid = valid_reg,
			.data = data_reg
		};
		*ready = *ret.ready | !valid_reg;
		return ret;
	}

	template<Signal Payload>
	inline Stream<Payload> Stream<Payload>::regUpstream(const RegisterSettings& settings)
	{
		Bit valid_reg;
		Payload data_reg = constructFrom(data);

		// we are ready as long as our buffer is unused
		*ready = !valid_reg;

		Stream<Payload> ret = {
			.valid = valid,
			.data = data
		};

		IF(*ret.ready)
			valid_reg = '0';

		IF(!valid_reg)
		{
			IF(!*ret.ready)
				valid_reg = valid;
			data_reg = data;
		}

		valid_reg = reg(valid_reg, '0', settings);
		data_reg = reg(data_reg, settings);

		IF(valid_reg)
		{
			ret.valid = '1';
			ret.data = data_reg;
		}
		return ret;
	}

	template<Signal Payload>
	inline Stream<Payload> Stream<Payload>::fifo(size_t minDepth)
	{
		Fifo<Payload> inst{ minDepth, data };
		Stream ret = fifo(inst);
		inst.generate();
		return ret;
	}

	template<Signal Payload>
	inline Stream<Payload> gtry::scl::Stream<Payload>::fifo(Fifo<Payload>& instance)
	{
		connect(instance, *this);

		Stream<Payload> ret = constructFrom(*this);
		connect(ret, instance);
		return ret;
	}

	template<Signal T>
	Stream<T> reg(Stream<T>& stream, const RegisterSettings& settings)
	{
		//return stream.regDownstreamBlocking(settings).regUpstream(settings);
		return stream.regUpstream(settings).regDownstream(settings);
	}

	template<Signal T>
	void connect(Fifo<T>& sink, Stream<T>& source)
	{
		IF(transfer(source))
			sink.push(*source);
		*source.ready = !sink.full();
	}

	template<Signal T>
	void connect(Stream<T>& sink, Fifo<T>& source)
	{
		sink.valid = !source.empty();
		sink.data = source.peak();

		IF(transfer(sink))
			source.pop();
	}
}
