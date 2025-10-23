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
#include <cstdint>
#include <deque>
#include <vector>

namespace gtry::hlim 
{
	struct LutTree
	{
		using NodeId = std::uint32_t;

		enum class Fun : std::uint8_t
		{
			Zero	= 0b0000,
			Nor		= 0b0001,
			Cmp1	= 0b0010,	// input equal 1
			InvB	= 0b0011,
			Cmp2	= 0b0100,	// input equal 2
			InvA	= 0b0101,
			Xor		= 0b0110,
			Nand	= 0b0111,
			And		= 0b1000,
			Xnor	= 0b1001,
			A		= 0b1010,
			CmpN2	= 0b1011,	// input not equal 2
			B		= 0b1100,
			CmpN1	= 0b1101,	// input not equal 1
			Or		= 0b1110,
			One		= 0b1111,
		};

		struct Node
		{
			Fun fun = Fun::Zero;
			NodeId in[2] = {};
		};

		std::deque<Node> nodes = { Node{ Fun::Zero }, Node{ Fun::One } };
		std::vector<NodeId> inNodes;
		std::vector<NodeId> outNodes;

		size_t setOutput(NodeId id);
		std::pair<NodeId, size_t> addInput();

		inline NodeId addNode(Fun fun, NodeId a = 0, NodeId b = 0) { nodes.emplace_back(Node{ fun, {a, b} }); return NodeId(nodes.size() - 1); }
		NodeId addNodeTree(Fun fun, std::span<const NodeId> in);

		NodeId addCompareConst(std::span<const NodeId> in, uint64_t val);
		NodeId addMux(std::span<const NodeId> select, std::span<const NodeId> in);

		// the following are convinience functions to add LUT's iff they cannot be trivially optimized
		NodeId addAnd(NodeId a, NodeId b);
		NodeId addOr(NodeId a, NodeId b);
		NodeId addXor(NodeId a, NodeId b);
	};

	inline LutTree::Fun operator ! (LutTree::Fun f) { return LutTree::Fun(uint8_t(f) ^ 0b1111); }


	class LutTreeEvaluation
	{
	public:
		LutTreeEvaluation() = default;
		LutTreeEvaluation(const LutTree& t) { tree(t); }

		LutTreeEvaluation& tree(const LutTree& tree);

		LutTreeEvaluation& input(size_t index, size_t value);
		LutTreeEvaluation& evaluate();
		size_t output(size_t index) const;

	protected:
		uint8_t getBit(LutTree::NodeId id) const;
		void setBit(LutTree::NodeId id, uint8_t value);

	private:
		const LutTree* m_tree = nullptr;
		std::vector<uint64_t> m_state;
		std::vector<LutTree::NodeId> m_evalOrder;
	};
}
