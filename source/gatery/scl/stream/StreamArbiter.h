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

#include "Stream.h"
#include "../utils/OneHot.h"

namespace gtry::scl
{
	template<typename T, typename TSelector = SelectorLowest>
	class StreamArbiter
	{
	public:
		StreamArbiter(TSelector&& selector = TSelector{}) : m_selector(selector) {}
		~StreamArbiter() noexcept(false)
		{
			HCL_DESIGNCHECK_HINT(m_generated, "Generate not called.");
		}

		void out(const T& blueprint)
		{
			HCL_DESIGNCHECK_HINT(!m_generated, "Already generated.");
			m_out.emplace(constructFrom(blueprint));
		}

		void attach(Stream<T>& stream, uint32_t sortKey = 1u << 31)
		{
			HCL_DESIGNCHECK_HINT(!m_generated, "Already generated.")

			InStream& s = m_in.emplace_back(InStream{
				.sortKey = sortKey
			});
			s.stream <<= stream;

			if(!m_out)
				m_out.emplace(constructFrom(stream));
		}

		Stream<T>& out() { return *m_out; }

		virtual void generate()
		{
			auto area = Area{ "scl_StreamArbiter" }.enter();
			HCL_DESIGNCHECK_HINT(m_out, "No input stream attached and out template not provided.");
			HCL_DESIGNCHECK_HINT(!m_generated, "Generate called twice.")
			m_generated = true;

			m_in.sort([](InStream& a, InStream& b) { return a.sortKey < b.sortKey; });

			UInt selected = m_selector(m_in);
			HCL_NAMED(selected);
			m_out->valid = '0';
			m_out->data = dontCare(m_out->data);

			HCL_NAMED(m_in);
			size_t i = 0;
			for(InStream& s : m_in)
			{
				*s.stream.ready = '0';
				IF(selected == i++)
					*m_out <<= s.stream;
			}
			HCL_NAMED(m_out);
		}

	protected:
		struct InStream
		{
			BOOST_HANA_DEFINE_STRUCT(InStream,
				(uint32_t, sortKey),
				(Stream<T>, stream)
			);
		};
		std::list<InStream> m_in;
		std::optional<Stream<T>> m_out;
		TSelector m_selector;
		bool m_generated = false;
	};

	namespace internal
	{
		template<typename T>
		UInt gatherValids(const T& container)
		{
			UInt mask = ConstUInt(BitWidth{ container.size() });
			size_t i = 0;
			for(const auto& it : container)
				mask[i++] = it.stream.valid;
			HCL_NAMED(mask);
			return mask;
		}
	}

	template<typename TSelector>
	struct SelectorReg : TSelector
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			return reg(TSelector::operator ()(in), 0);
		}
	};

	struct SelectorLowest
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			auto [value, valid] = scl::priorityEncoder(internal::gatherValids(in));
			IF(!valid)
				value = 0;
			return value;
		}
	};

	template<typename T>
	struct arbitrateInOrder : Stream<T>
	{
		arbitrateInOrder(Stream<T>& in0, Stream<T>& in1);
	};

	template<typename T>
	inline arbitrateInOrder<T>::arbitrateInOrder(Stream<T>& in0, Stream<T>& in1)
	{
		auto entity = Area{ "arbitrateInOrder" }.enter();

		*in0.ready = *Stream<T>::ready;
		*in1.ready = *Stream<T>::ready;

		// simple fsm state 0 is initial and state 1 is push upper input
		Bit selectionState;
		HCL_NAMED(selectionState);

		Stream<T>::data = in0.data;
		Stream<T>::valid = in0.valid;
		IF(selectionState == '1' | !in0.valid)
		{
			Stream<T>::data = in1.data;
			Stream<T>::valid = in1.valid;
		}

		IF(*Stream<T>::ready)
		{
			IF(	selectionState == '0' & 
				in0.valid & in1.valid)
			{
				selectionState = '1';
			}
			ELSE
			{
				selectionState = '0';
			}

			IF(selectionState == '1')
			{
				*in0.ready = '0';
				*in1.ready = '0';
			}
		}
		selectionState = reg(selectionState, '0');
	}
}
