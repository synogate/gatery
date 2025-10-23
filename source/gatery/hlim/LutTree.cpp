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
#include "gatery/pch.h"
#include "LutTree.h"
#include "../utils/Exceptions.h"

namespace gtry::hlim 
{
	size_t LutTree::setOutput(NodeId id)
	{
		outNodes.push_back(id);
		return outNodes.size() - 1;
	}

	std::pair<LutTree::NodeId, size_t> LutTree::addInput()
	{
		NodeId id = addNode(Fun::Zero);
		inNodes.push_back(id);

		return std::pair<NodeId, size_t>(
			id, inNodes.size() - 1
		);
	}

	LutTree::NodeId LutTree::addNodeTree(Fun fun, std::span<const NodeId> in)
	{
		if (in.empty())
			return 0;
		if (in.size() == 1)
			return in.front();
		if (in.size() == 2)
			return addNode(fun, in.front(), in.back());

		size_t split = in.size() / 2;
		return addNode(fun,
			addNodeTree(fun, in.subspan(0, split)),
			addNodeTree(fun, in.subspan(split))
		);
	}

	LutTree::NodeId LutTree::addCompareConst(std::span<const NodeId> in, uint64_t val)
	{
		if (in.size() == 0)
			return 1; // always true
		if (in.size() == 1)
		{
			if (val)
				return in.front();
			else // special case not used in recursion to prevent inverter nodes
				return addNode(Fun::InvA, in.front());
		}
		if (in.size() <= 3)
		{
			Fun compare1 = Fun(1ull << (val & 0b011));
			NodeId ret = addNode(compare1, in[0], in[1]);
			if (in.size() == 3)
			{
				Fun compare2 = (val & 0b100) ? Fun::And : Fun::Cmp1;
				ret = addNode(compare2, ret, in[2]);
			}
			return ret;
		}
		size_t split = in.size() / 2;
		return addNode(Fun::And,
			addCompareConst(in.subspan(0, split), val),
			addCompareConst(in.subspan(split), val >> split)
		);
	}

	LutTree::NodeId LutTree::addMux(std::span<const NodeId> select, std::span<const NodeId> in)
	{
		HCL_ASSERT(1ull << select.size() >= in.size());
		std::vector<NodeId> orTree(in.size());

		for (size_t i = 0; i < in.size(); ++i)
			orTree.push_back(addNode(Fun::And, in[i], addCompareConst(select, i)));

		return addNodeTree(Fun::Or, orTree);
	}

	LutTreeEvaluation& LutTreeEvaluation::tree(const LutTree& tree)
	{
		m_tree = &tree;
		m_state.resize((tree.nodes.size() + 63) / 64);
		m_evalOrder.reserve(tree.nodes.size());

		std::deque<LutTree::NodeId> openList;
		for (LutTree::NodeId o : tree.outNodes)
			openList.emplace_back(o);

		while (!openList.empty())
		{
			const LutTree::NodeId id = openList.front();
			const LutTree::Node& n = tree.nodes[id];
			openList.pop_front();
			
			if (n.fun == LutTree::Fun::Zero)
				continue;

			// check and mark already visited
			if (getBit(id))
				continue;
			setBit(id, 1);

			m_evalOrder.push_back(id);
			for (LutTree::NodeId c : n.in)
				if (c)
					openList.push_back(c);
		}

		setBit(0, 0);
		setBit(1, 1);
		return *this;
	}

	LutTreeEvaluation& LutTreeEvaluation::input(size_t index, size_t value)
	{
		HCL_ASSERT(m_tree);
		HCL_ASSERT(index < m_tree->inNodes.size());
		setBit(m_tree->inNodes[index], uint8_t(value));
		return *this;
	}

	LutTreeEvaluation& LutTreeEvaluation::evaluate()
	{
		for (auto it = m_evalOrder.crbegin(); it != m_evalOrder.crend(); ++it)
		{
			const LutTree::Node& n = m_tree->nodes[*it];
			uint8_t a = getBit(n.in[0]);
			uint8_t b = getBit(n.in[1]);
			uint8_t c = (uint8_t(n.fun) >> (a + 2 * b)) & 1;
			setBit(*it, c);
		}
		return *this;
	}

	size_t LutTreeEvaluation::output(size_t index) const
	{
		HCL_ASSERT(m_tree);
		HCL_ASSERT(index < m_tree->outNodes.size());
		return getBit(m_tree->outNodes[index]);
	}

	uint8_t LutTreeEvaluation::getBit(LutTree::NodeId id) const
	{
		HCL_ASSERT(id / 64 < m_state.size());
		return uint8_t((m_state[id / 64] >> (id % 64)) & 1);
	}

	void LutTreeEvaluation::setBit(LutTree::NodeId id, uint8_t value)
	{
		HCL_ASSERT(id / 64 < m_state.size());
		m_state[id / 64] = (m_state[id / 64] & ~(1ull << (id % 64))) | (uint64_t(value & 1) << (id % 64));
	}

	LutTree::NodeId LutTree::addAnd(NodeId a, NodeId b)
	{
		if (a == 1)
			return b;
		if (b == 1)
			return a;

		return addNode(Fun::And, a, b);
	}

	LutTree::NodeId LutTree::addOr(NodeId a, NodeId b)
	{
		if (a == 0)
			return b;
		if (b == 0)
			return a;

		return addNode(Fun::Or, a, b);
	}

	LutTree::NodeId LutTree::addXor(NodeId a, NodeId b)
	{
		if (a == b)
			return 0;

		return addNode(Fun::Xor, a, b);
	}
}
