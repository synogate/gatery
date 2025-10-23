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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>



using namespace boost::unit_test;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

using namespace gtry::hlim;

BOOST_AUTO_TEST_CASE(LutTreeTest_Constant)
{
	LutTree tree;
	std::unordered_map<NodePortBit<Out>, LutTree::NodeId> portMap;

	gtry::sim::DefaultBitVectorState value;
	value.resize(3);
	*value.data(gtry::sim::DefaultConfig::VALUE) = 0b110;
	*value.data(gtry::sim::DefaultConfig::DEFINED) = 0b011;

	Node_Constant node(value, gtry::hlim::ConnectionType::BITVEC);
	node.generateLutTree(tree, portMap);

	BOOST_TEST(portMap.size() == 3);
	BOOST_TEST(tree.nodes.size() == 2);
}

BOOST_AUTO_TEST_CASE(LutTreeTest_Logic)
{
	LutTree tree;
	std::unordered_map<NodePortBit<Out>, LutTree::NodeId> portMap;

	gtry::sim::DefaultBitVectorState value;
	value.resize(3);
	*value.data(gtry::sim::DefaultConfig::VALUE) = 0b110;
	*value.data(gtry::sim::DefaultConfig::DEFINED) = 0b011;

	Node_Constant const0(value, gtry::hlim::ConnectionType::BITVEC);
	const0.generateLutTree(tree, portMap);

	Node_Logic node(Node_Logic::Op::XOR);
	node.connectInput(0, p0(&const0));
	node.connectInput(1, p0(&const0));	
	node.generateLutTree(tree, portMap);

	BOOST_TEST(portMap.size() == 6);
	BOOST_TEST(tree.nodes.size() == 2 + 3);
}

BOOST_AUTO_TEST_CASE(LutTreeTest_Rewire)
{
	LutTree tree;
	std::unordered_map<NodePortBit<Out>, LutTree::NodeId> portMap;

	gtry::sim::DefaultBitVectorState value;
	value.resize(3);
	*value.data(gtry::sim::DefaultConfig::VALUE) = 0b110;
	*value.data(gtry::sim::DefaultConfig::DEFINED) = 0b011;

	Node_Constant const0(value, gtry::hlim::ConnectionType::BITVEC);
	const0.generateLutTree(tree, portMap);

	Node_Rewire node(1);
	node.connectInput(0, p0(&const0));
	node.setOp({ {
		Node_Rewire::OutputRange{1, Node_Rewire::OutputRange::INPUT, 0, 1},
		Node_Rewire::OutputRange{2, Node_Rewire::OutputRange::CONST_ONE},
		Node_Rewire::OutputRange{3, Node_Rewire::OutputRange::CONST_ZERO},
	} });
	node.generateLutTree(tree, portMap);

	BOOST_TEST(portMap.size() == 3 + 6);
	BOOST_TEST(tree.nodes.size() == 2);
}

BOOST_AUTO_TEST_CASE(LutTreeTest_EvalMux)
{
	LutTree tree;
	
	std::vector<LutTree::NodeId> in;
	for (size_t i = 0; i < 64; ++i)
		in.push_back(tree.addInput().first);

	std::vector<LutTree::NodeId> sel;
	for (size_t i = 0; i < 6; ++i)
		sel.push_back(tree.addInput().first);

	tree.setOutput(tree.addMux(sel, in));

	LutTreeEvaluation eval{ tree };

	uint64_t testValue = uint64_t(std::random_device{}()) << 32 ^ std::random_device{}();
	for (size_t i = 0; i < 64; ++i)
		eval.input(i, testValue >> i);

	for (size_t i = 0; i < 64; ++i)
	{
		for (size_t j = 0; j < 6; ++j)
			eval.input(64 + j, i >> j);

		eval.evaluate();
		BOOST_TEST(eval.output(0) == ((testValue >> i) & 1));
	}


}
