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

#include "../utils/ConfigTree.h"

#include <set>
#include <span>

namespace gtry::hlim {

class BaseNode;
struct NodePort;
class Circuit;
class NodeGroup;

class ConstSubnet;

template<bool makeConst, typename FinalType>
class SubnetTemplate {
	public:
		template<bool, typename baseType>
		struct ConstAdaptor { typedef baseType type; };
		template<typename baseType>
		struct ConstAdaptor<true, baseType> { typedef const baseType type; };

		typedef typename ConstAdaptor<makeConst, Circuit>::type CircuitType;
		typedef typename ConstAdaptor<makeConst, BaseNode>::type NodeType;


		static FinalType all(CircuitType &circuit);
		static FinalType allDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs);
		static FinalType allNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs);
		static FinalType allNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes);
		static FinalType allDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs);
		static FinalType allForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs = {}, bool includeRefed = true);
		static FinalType allForExport(CircuitType &circuit, const utils::ConfigTree &exportSelectionConfig = {});
		static FinalType allUsedNodes(CircuitType &circuit);
		static FinalType fromNodeGroup(NodeGroup *nodeGroup, bool reccursive = true);

		/// Adds just a single node
		FinalType &add(NodeType *node);

		/// Removes a single node
		FinalType &remove(NodeType *node);

		/// Adds everything that is driven directly or indirectly by the specified outputs, stopping at the limiting input or at dead ends like output pins or signal taps
		/// @details A subset specified by this function does include nodes with potential side effects that don't seem to contribute to the limiting inputs.
		FinalType &addAllDrivenByOutputs(std::span<NodePort> outputs, std::span<NodePort> limitingInputs);
		/// Adds everything that drives directly or indirectly the specified inputs, stopping at the limiting outputs or at dead ends like input pins
		/// @details A subset specified by this function does NOT include nodes with potential side effects that don't seem to contribute to the inputs.
		FinalType &addAllNecessaryForInputs(std::span<NodePort> limitingOutputs, std::span<NodePort> inputs);
		/// Adds everything that drives directly or indirectly the specified nodes, stopping at the limiting nodes or at dead ends like input pins
		/// @details A subset specified by this function does NOT include nodes with potential side effects that don't seem to contribute to the inputs.
		FinalType &addAllNecessaryForNodes(std::span<NodeType*> limitingNodes, std::span<NodeType*> nodes);
		/// Adds everything that is driven directly (combinatorically) by the specified outputs
		FinalType &addAllDrivenCombinatoricallyByOutputs(std::span<NodePort> outputs);

		/// Adds all nodes
		FinalType &addAll(CircuitType &circuit);
		/// Adds all nodes that a simulation might want (skips export side of export overrides)
		FinalType &addAllForSimulation(CircuitType &circuit, const std::set<hlim::NodePort> &outputs = {}, bool includeRefed = true);
		/// Adds all nodes that an export might want (skips simulation side of export overrides)
		FinalType &addAllForExport(CircuitType &circuit, const utils::ConfigTree &exportSelectionConfig = {});
		/// Adds all nodes that are used or have side effects
		FinalType &addAllUsedNodes(CircuitType &circuit);

		/// Adds all nodes from a node group (potentially reccursive)
		FinalType &addAllFromNodeGroup(NodeGroup* nodeGroup, bool reccursive = true);


		ConstSubnet &asConst() const { return (ConstSubnet&)*this; }

		void dilate(bool forward, bool backward);

		inline const std::set<NodeType*> &getNodes() const { return m_nodes; }

		inline bool contains(NodeType *node) const { return m_nodes.contains(node); }
		inline bool empty() const { return m_nodes.empty(); }

		auto begin() const { return m_nodes.begin(); }
		auto end() const { return m_nodes.end(); }
		operator std::set<NodeType*>() const { return m_nodes; }
	protected:
		std::set<NodeType*> m_nodes;
};

class Subnet : public SubnetTemplate<false, Subnet> { };
class ConstSubnet : public SubnetTemplate<true, ConstSubnet> { };

extern template class SubnetTemplate<false, Subnet>;
extern template class SubnetTemplate<true, ConstSubnet>;

}
