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

	template<StreamSignal T, typename TSelector = ArbiterPolicyLowest>
	class StreamArbiter
	{
	public:
		StreamArbiter(TSelector&& selector = TSelector{}) : m_selector(selector) {}
		~StreamArbiter() noexcept(false)
		{
			HCL_DESIGNCHECK_HINT(m_generated, "Generate not called.");
		}

		//void out(const T& blueprint)
		//{
		//	HCL_DESIGNCHECK_HINT(!m_generated, "Already generated.");
		//	m_out.emplace(constructFrom(blueprint));
		//}

		void attach(T& stream, uint32_t sortKey = 1u << 31)
		{
			HCL_DESIGNCHECK_HINT(!m_generated, "Already generated.")

			InStream& s = m_in.emplace_back(InStream{
				.sortKey = sortKey
			});
			s.stream <<= stream;

			if (!m_out)
			{
				m_out.emplace();
				downstream(*m_out) = constructFrom(copy(downstream(stream)));
			}
		}

		void attach(T&& stream, uint32_t sortKey = 1u << 31) { attach(stream, sortKey); }

		void name(std::string name) { m_areaName = name; }

		T& out() { return *m_out; }

		virtual void generate()
		{
			auto area = Area{ m_areaName }.enter();
			HCL_DESIGNCHECK_HINT(m_out, "No input stream attached and out template not provided.");
			HCL_DESIGNCHECK_HINT(!m_generated, "Generate called twice.")
			m_generated = true;

			m_in.sort([](InStream& a, InStream& b) { return a.sortKey < b.sortKey; });

			Bit locked = flag(transfer(*m_out), eop(*m_out) & valid(*m_out));
			HCL_NAMED(locked);

			UInt selected = BitWidth::count(m_in.size());
			selected = reg(selected, 0);
			IF(!locked & reg(ready(*m_out) | !valid(*m_out), '1'))
#ifdef __clang__
				{
					std::list<std::reference_wrapper<T>> streamsOnly;
					for (auto &s : m_in) streamsOnly.push_back(s.stream);
					selected = m_selector(streamsOnly);
				}
#else			
				selected = m_selector(m_in | std::views::transform(&InStream::stream));
#endif				
			HCL_NAMED(selected);
			m_selectedInput = selected;

			downstream(*m_out) = dontCare(copy(downstream(*m_out)));
			valid(*m_out) = '0';

			HCL_NAMED(m_in);
			size_t i = 0;
			for(InStream& s : m_in)
			{
				ready(s.stream) = '0';
				IF(selected == i++)
					*m_out <<= s.stream;
			}
			HCL_NAMED(m_out);
		}

		const UInt& selectedInput() const { 
			HCL_DESIGNCHECK(m_generated);
			return m_selectedInput; 
		}

	protected:
		struct InStream
		{
			BOOST_HANA_DEFINE_STRUCT(InStream,
				(uint32_t, sortKey),
				(T, stream)
			);
		};
		std::list<InStream> m_in;
		std::optional<T> m_out;
		std::string m_areaName = "scl_StreamArbiter";
		TSelector m_selector;
		bool m_generated = false;

	private:
		UInt m_selectedInput;
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
#ifdef __clang__
			std::vector<Bit> valids;
			for (const auto &s : in)
				valids.emplace_back(valid(s.get()));
			UInt mask = cat(valids);
#else		
			UInt mask = cat(in | views::valid);
#endif
			auto idx = scl::priorityEncoder(mask);
			IF(!valid(idx))
				*idx = 0;
			return *idx;
		}
	};

	struct ArbiterPolicyRoundRobin
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			auto scope = Area{ "RoundRobin" }.enter();
#ifdef __clang__
			std::vector<Bit> valids;
			for (const auto &s : in)
				valids.emplace_back(valid(s.get()));
			UInt mask = cat(valids);
#else		
			UInt mask = cat(in | views::valid);
#endif

			UInt counter = Counter{ mask.size() }.inc().value();
			HCL_NAMED(counter);

			mask = rotr(mask, counter);
			HCL_NAMED(mask);

			auto idx = scl::priorityEncoder(mask);
			IF(!valid(idx))
				*idx = 0;
			HCL_NAMED(idx);

			UInt selected = zext(*idx, +1_b) + zext(counter, +1_b);
			IF(selected >= mask.size())
				selected -= mask.size();
			HCL_NAMED(selected);

			return selected(0, -1_b);
		}
	};

	struct ArbiterPolicyRoundRobinBubble
	{
		template<class TCont>
		UInt operator () (const TCont& in)
		{
			return Counter{ in.size() }.inc().value();
		}
	};

	struct ArbiterPolicyExtern
	{
		UInt selection;

		template<class TCont>
		UInt operator () (const TCont& in)
		{
			return selection;
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
