/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

namespace gtry::hlim {

	class Circuit;
	struct NodePort;
	struct RefCtdNodePort;

	class Node_Signal;
	class Subnet;
	class NodeGroup;
	class Clock;
	class Node_Attributes;
	class BaseNode;
	class Node_Constant;
	class Node_Register;
	class Node_Multiplexer;
	class Node_Logic;
	class Node_Compare;
	class Node_Arithmetic;


    class ConstructionHelper {
		public:
			ConstructionHelper(Circuit &circuit);
			ConstructionHelper &constructInGroup(NodeGroup &nodeGroup);
			ConstructionHelper &newNodesToSubnet(Subnet &subnet);

			Node_Constant *constBoolean(bool b);
			Node_Constant *constBVec(std::uint64_t value, size_t width);

			Node_Register *reg(Clock &clock, const NodePort& data);
			Node_Register *reg(Clock &clock, const NodePort& data, const NodePort& resetValue);
			Node_Multiplexer *mux(const NodePort& sel, const NodePort& sel0, const NodePort& sel1);

			Node_Signal* appendSignal(NodePort& nodePort);
			Node_Signal* appendSignal(RefCtdNodePort& nodePort);

			NodePort isInReset(Clock &clock);

			Node_Logic *land(const NodePort &lhs, const NodePort &rhs);
			Node_Logic *lor(const NodePort &lhs, const NodePort &rhs);
			Node_Logic *lnot(const NodePort &src);

			Node_Compare *ceq(const NodePort &lhs, const NodePort &rhs);
			Node_Compare *cneq(const NodePort &lhs, const NodePort &rhs);

			Node_Arithmetic *aadd(const NodePort &lhs, const NodePort &rhs);
			Node_Arithmetic *asub(const NodePort &lhs, const NodePort &rhs);

			Node_Attributes* getCreateAttribNode(NodePort& nodePort);

		protected:
			Circuit &m_circuit;
			NodeGroup *m_nodeGroup = nullptr;
			Subnet *m_subnet = nullptr;

			void newNode(BaseNode *node, NodeGroup *defaultNodeGroup = nullptr);
	};
}