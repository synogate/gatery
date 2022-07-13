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
#include <ranges>

#include "Stream.h"
#include "../utils/OneHot.h"
#include "../views.h"
#include "../Counter.h"

namespace gtry::scl
{
	struct ArbiterPolicyLowest;

	template<typename T, typename TSelector = ArbiterPolicyLowest>
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

			Bit locked = flag(transfer(*m_out), eop(*m_out) & valid(*m_out));
			HCL_NAMED(locked);

			UInt selected = BitWidth::count(m_in.size());
			selected = reg(selected, 0);
			IF(!locked & reg(ready(*m_out) | !valid(*m_out), '1'))
				selected = m_selector(m_in | std::views::transform(&InStream::stream));
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

	template<typename TSelector>
	struct ArbiterPolicyReg : TSelector
	{
		using TSelector::TSelector;

		template<class TCont>
		UInt operator () (const TCont& in)
		{
			return reg(TSelector::operator ()(in), 0);
		}
	};

	struct ArbiterPolicyLowest
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			UInt mask = cat(in | views::valid);
			auto [valid, value] = scl::priorityEncoder(mask);
			IF(!valid)
				value = 0;
			return value;
		}
	};

	struct ArbiterPolicyRoundRobin
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			auto scope = Area{ "RoundRobin" }.enter();
			UInt mask = cat(in | views::valid);

			UInt counter = Counter{ mask.size() }.value();
			HCL_NAMED(counter);

			mask = rotr(mask, counter);
			HCL_NAMED(mask);

			auto [valid, firstValid] = scl::priorityEncoder(mask);
			IF(!valid)
				firstValid = 0;
			HCL_NAMED(firstValid);

			UInt selected = zext(firstValid, 1) + zext(counter, 1);
			IF(selected >= mask.size())
				selected -= mask.size();
			HCL_NAMED(selected);

			return selected(0, -1);
		}
	};

	struct ArbiterPolicyRoundRobinBubble
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			return Counter{ in.size() }.value();
		}
	};

	template<typename T>
	struct arbitrateInOrder : RvStream<T>
	{
		arbitrateInOrder(RvStream<T>& in0, RvStream<T>& in1);
	};

	template<typename T>
	inline arbitrateInOrder<T>::arbitrateInOrder(RvStream<T>& in0, RvStream<T>& in1)
	{
		auto entity = Area{ "arbitrateInOrder" }.enter();

		RvStream<T>& me = *this;
		ready(in0) = ready(me);
		ready(in1) = ready(me);

		// simple fsm state 0 is initial and state 1 is push upper input
		Bit selectionState;
		HCL_NAMED(selectionState);

		*me = *in0;
		valid(me) = valid(in0);
		IF(selectionState == '1' | !valid(in0))
		{
			*me = *in1;
			valid(me) = valid(in1);
		}

		IF(ready(me))
		{
			IF(	selectionState == '0' & 
				valid(in0) & valid(in1))
			{
				selectionState = '1';
			}
			ELSE
			{
				selectionState = '0';
			}

			IF(selectionState == '1')
			{
				ready(in0) = '0';
				ready(in1) = '0';
			}
		}
		selectionState = reg(selectionState, '0');
	}
}
