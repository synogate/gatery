#pragma once
#include "Compound.h"

#include <hcl/hlim/SignalGroup.h>

namespace hcl::core::frontend
{
	template<typename... Comp>
	BVec pack(const Comp& ...compound)
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

			std::deque<SignalReadPort> m_ports;
		};

		PackVisitor v;
		(visitForcedSignalCompound(compound, v), ...);

		auto* m_node = DesignScope::createNode<hlim::Node_Rewire>(v.m_ports.size());
		for (size_t i = 0; i < v.m_ports.size(); ++i)
			m_node->connectInput(i, v.m_ports[v.m_ports.size() - 1 - i]);
		m_node->setConcat();
		return SignalReadPort(m_node);
	}
	
	template<typename... Comp>
	void unpack(const BVec& vec, Comp& ... compound)
	{
		struct UnpackVisitor : CompoundVisitor
		{
			UnpackVisitor(const BVec& out) : 
				m_packed(out),
				m_totalWidth(out.size())
			{}

			virtual void operator () (BVec& vec, const BVec&) override
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				m_totalWidth -= vec.size();
				node->setExtract(m_totalWidth, vec.getWidth());

				vec = BVec{ SignalReadPort(node) };
			}

			virtual void operator () (Bit& vec, const Bit&) override
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				node->changeOutputType({ hlim::ConnectionType::BOOL });
				m_totalWidth--;
				node->setExtract(m_totalWidth, 1);

				vec = Bit{ SignalReadPort(node) };
			}

			const BVec& m_packed;
			size_t m_totalWidth = 0;
		};

		HCL_DESIGNCHECK(vec.getWidth() == width(compound...));

		UnpackVisitor v{ vec };
		(VisitCompound<Comp>{}(compound, compound, v, 0), ...);
	}
}
