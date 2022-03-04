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
#include "Compound.h"

#include <gatery/hlim/SignalGroup.h>

namespace gtry
{
	namespace internal
	{
		void ports(const TupleSignal auto& signal, std::vector<SignalReadPort>& list);
		void ports(const CompoundSignal auto& signal, std::vector<SignalReadPort>& list);

		void ports(const BaseSignal auto& signal, std::vector<SignalReadPort>& list)
		{
			list.push_back(signal.getReadPort());
		}

		void ports(const BaseSignalLiteral auto& value, std::vector<SignalReadPort>& list)
		{
			ports(ValueToBaseSignal<decltype(value)>{value}, list);
		}

		void ports(const ContainerSignal auto& signal, std::vector<SignalReadPort>& list)
		{
			for(auto&& it : signal)
				ports(it, list);
		}

		void ports(const TupleSignal auto& signal, std::vector<SignalReadPort>& list)
		{
			boost::hana::for_each(signal, [&](auto&& member) {
				if constexpr(Signal<decltype(member)>)
					ports(member, list);
			});
		}

		void ports(const CompoundSignal auto& signal, std::vector<SignalReadPort>& list)
		{
			ports(boost::pfr::structure_tie(signal), list);
		}

		inline void ports_reverse(std::vector<SignalReadPort>& list) {}

		template<typename T, typename... Tl>
		void ports_reverse(std::vector<SignalReadPort>& list, const T& signal, const Tl& ...other)
		{
			ports_reverse(list, other...);
			ports(signal, list);
		}
	}

	UInt pack(const SignalValue auto& ...compound)
	{
		std::vector<SignalReadPort> portList;
		internal::ports_reverse(portList, compound...);

		auto* m_node = DesignScope::createNode<hlim::Node_Rewire>(portList.size());
		for (size_t i = 0; i < portList.size(); ++i)
			m_node->connectInput(i, portList[i]);
		m_node->setConcat();
		return SignalReadPort(m_node);
	}

	namespace internal
	{
		struct UnpackVisitor : CompoundVisitor
		{
			UnpackVisitor(const UInt& out) : 
				m_packed(out)
			{}

			virtual void operator () (UInt& vec, const UInt&) override
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				node->setExtract(m_totalWidth, vec.size());
				m_totalWidth += vec.size();

				vec = UInt{ SignalReadPort(node) };
			}

			virtual void operator () (Bit& vec, const Bit&) override
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				node->changeOutputType({ hlim::ConnectionType::BOOL });
				node->setExtract(m_totalWidth, 1);
				m_totalWidth++;

				vec = Bit{ SignalReadPort(node) };
			}

			const UInt& m_packed;
			size_t m_totalWidth = 0;
		};

		inline void unpack(UnpackVisitor&) {}

		template<typename... Comp, typename T>
		void unpack(UnpackVisitor& v, T& compound, Comp& ...compoundList)
		{
			unpack(v, compoundList...);
			VisitCompound<T>{}(compound, compound, v, 0);
		}
	}
	
	template<typename... Comp>
	void unpack(const UInt& vec, Comp& ... compound)
	{
		HCL_DESIGNCHECK(vec.getWidth() == width(compound...));

		internal::UnpackVisitor v{ vec };
		internal::unpack(v, compound...);
	}
}
