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
		struct PackVisitor : CompoundVisitor
		{
			virtual void operator () (const BVec& a, const BVec&) override
			{
				m_ports.emplace_back(a.getReadPort());
			}

			virtual void operator () (const Bit& a, const Bit&) override
			{
				m_ports.emplace_back(a.getReadPort());
			}

			std::vector<SignalReadPort> m_ports;
		};

		inline void pack(PackVisitor&) {}

		template<typename... Comp, typename T>
		void pack(PackVisitor& v, const T& compound, const Comp& ...compoundList)
		{
			pack(v, compoundList...);
			visitForcedSignalCompound(compound, v);
		}
	}

	template<typename... Comp>
	BVec pack(const Comp& ...compound)
	{
		internal::PackVisitor v;
		internal::pack(v, compound...);

		auto* m_node = DesignScope::createNode<hlim::Node_Rewire>(v.m_ports.size());
		for (size_t i = 0; i < v.m_ports.size(); ++i)
			m_node->connectInput(i, v.m_ports[i]);
		m_node->setConcat();
		return SignalReadPort(m_node);
	}

	namespace internal
	{
		struct UnpackVisitor : CompoundVisitor
		{
			UnpackVisitor(const BVec& out) : 
				m_packed(out)
			{}

			virtual void operator () (BVec& vec, const BVec&) override
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				node->setExtract(m_totalWidth, vec.size());
				m_totalWidth += vec.size();

				vec = BVec{ SignalReadPort(node) };
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

			const BVec& m_packed;
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
	void unpack(const BVec& vec, Comp& ... compound)
	{
		HCL_DESIGNCHECK(vec.getWidth() == width(compound...));

		internal::UnpackVisitor v{ vec };
		internal::unpack(v, compound...);
	}
}
