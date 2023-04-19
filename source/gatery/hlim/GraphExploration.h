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

#include <gatery/utils/StableContainers.h>

#include "NodePort.h"
#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include <stack>
#include <queue>
#include <set>

namespace gtry::hlim {

class BaseNode;
class NodeIO;


template<bool forward, typename Policy>
class Exploration {
	public:
		class iterator;

		class NodePortHandle {
			public:
				bool isSignal() const;
				
				template<typename NodeType>
				bool isNodeType() const{ return dynamic_cast<const NodeType*>(node()) != nullptr; }

				bool isBranchingForward();
				bool isBranchingBackward();

				void backtrack();

				BaseNode *node() { return m_nodePort.node; }
				const BaseNode *node() const { return m_nodePort.node; }
				/// When exploring forward the input port, when exploring backwards the output port.
				size_t port() const { return m_nodePort.port; }
				/// When exploring forward the input port, when exploring backwards the output port.
				NodePort nodePort() const { return m_nodePort; }
			protected:
				iterator &m_iterator;
				NodePort m_nodePort;

				NodePortHandle(iterator &iterator, NodePort nodePort);

				friend class iterator;
		};

		class iterator {
			public:
				using iterator_category = std::forward_iterator_tag;				

				iterator() : m_isEndIterator(true) { }
				iterator(bool skipDependencies, bool skipExportOnly, NodePort nodePort) : m_skipDependencies(skipDependencies), m_skipExportOnly(skipExportOnly), m_isEndIterator(false) { m_policy.init(nodePort); }

				iterator &operator++() { if (m_ignoreAdvance) m_ignoreAdvance = false; else m_policy.advance(m_skipDependencies, m_skipExportOnly); return *this; }
				bool operator!=(const iterator &rhs) const { HCL_ASSERT(rhs.m_isEndIterator); return !m_policy.done(); }
				NodePortHandle operator*() { return NodePortHandle(*this, m_policy.getCurrent()); }
				void backtrack() { m_policy.backtrack(); m_ignoreAdvance = true; }
			protected:
				bool m_skipDependencies;
				bool m_skipExportOnly;
				bool m_isEndIterator;
				Policy m_policy;
				bool m_ignoreAdvance = false;
		};
		
		Exploration(NodePort nodePort);

		Exploration<forward, Policy> skipDependencies() {
			Exploration<forward, Policy> res(m_nodePort);
			res.m_skipDependencies = true;
			return res;
		}
		Exploration<forward, Policy> skipExportOnly() {
			Exploration<forward, Policy> res(m_nodePort);
			res.m_skipExportOnly = true;
			return res;
		}

		iterator begin();
		iterator end();
	protected:
		bool m_skipDependencies = false;
		bool m_skipExportOnly = false;
		NodePort m_nodePort;
};

template<bool forward>
class DepthFirstPolicy {
	public:
		void init(NodePort nodePort);
		void advance(bool skipDependencies, bool skipExportOnly);
		void backtrack();
		bool done() const;
		NodePort getCurrent();
	protected:
		std::stack<NodePort> m_stack;
};

extern template class DepthFirstPolicy<true>;
extern template class DepthFirstPolicy<false>;

extern template class Exploration<true, DepthFirstPolicy<true>>;
extern template class Exploration<false, DepthFirstPolicy<false>>;

using ExplorationFwdDepthFirst = Exploration<true, DepthFirstPolicy<true>>;
using ExplorationBwdDepthFirst = Exploration<false, DepthFirstPolicy<false>>;



namespace DijkstraDetails {

template<class iterator, class OpenNode>
class BaseNodeHandle {
	public:
		template<typename NodeType>
		bool isNodeType() const { return dynamic_cast<const NodeType*>(node()) != nullptr; }

		BaseNode *node() { return m_top.nodePort.node; }
		const BaseNode *node() const { return m_top.nodePort.node; }

		size_t getDistance() const { return m_top.distance; }
	protected:
		iterator &m_iterator;
		const OpenNode &m_top;

		BaseNodeHandle(iterator &it, const OpenNode &top) : m_iterator(it), m_top(top) { }
};

template<class OpenNode>
class BaseIterator {
	public:
		using iterator_category = std::forward_iterator_tag;

		BaseIterator() { }
		BaseIterator(const std::vector<OpenNode> &start) { 
			for (const auto& s : start)
				m_openList.push(s);
		}

	protected:
		OpenNode m_current;

		struct NearestFirst {
			bool operator()(const OpenNode &lhs, const OpenNode &rhs) const {
				if (lhs.distance > rhs.distance) return true;
				if (lhs.distance < rhs.distance) return false;
				return utils::StableCompare<NodePort>()(lhs.nodePort, rhs.nodePort);
			}
		};
		std::priority_queue<OpenNode, std::vector<OpenNode>, NearestFirst> m_openList;
};

}


/**
 * @brief Forward enumeration of the graph nodes, ordered by distance, and visiting nodes only once.
 * @details This operates like a virtual container and is to be used in a ranged for loop. From a given set of sources, the range for loop will
 * explore the graph forwards. Nodes are enumerated through node handles which allow control of the exploration. Output ports to explore
 * or skip can be specified from each enumerated node handle. By default, all output ports are explored.
 * Exploration happens in a dijkstra-like manner with distances specified through the node handles (default zero).
 */
class DijkstraExploreNodesForward {
	public:
		struct OpenNode {
			size_t distance = 0;
			NodePort nodePort;
		};

		class iterator;

		class NodeHandle : public DijkstraDetails::BaseNodeHandle<iterator, OpenNode> {
			public:
				NodeHandle(iterator &iterator, const OpenNode &top) : BaseNodeHandle(iterator, top) { }

				/// @brief The input port through which the exploration arrived at this node.
				size_t inputPort() const { return m_top.nodePort.port; }
				/// @brief The input NodePort through which the exploration arrived at this node.
				NodePort input() const { return m_top.nodePort; }

				/**
				 * @brief Instructs the exploration to proceed to the output ports enabled in the proceedPortMask with the given cost.
				 * @details Can be called multiple times to give different costs to different output ports. By default, all output ports are explored with a cost of zero.
				 * @param cost The cost associated with the edges originating from the specified outputs.
				 * @param proceedPortMask Bit mask that indicates which output ports are to be explored with this cost.
				 */
				void proceed(size_t cost = 0, std::uint64_t proceedPortMask = ~0ull) { m_iterator.proceed(cost, proceedPortMask); }
				/**
				 * @brief Instructs the exploration to not do the default and proceed to all outputs. 
				 * @details Has no effect if proceed is called as well.
				 */
				void skip() { m_iterator.skip(); }
		};
		

		class iterator : public DijkstraDetails::BaseIterator<OpenNode> {
			public:
				using iterator_category = std::forward_iterator_tag;				

				iterator() { }
				iterator(const std::vector<OpenNode> &start);
				iterator &operator++();
				NodeHandle operator*() { return NodeHandle(*this, m_current); }
				void proceed(size_t cost, std::uint64_t proceedPortMask);
				bool operator!=(const iterator &rhs) const;
				void skip();
			protected:
				bool m_autoProceed = true;

				utils::UnstableSet<BaseNode*> m_closedList;

				void uncoverNext();
		};

		iterator begin() { return iterator(m_start); }
		iterator end() { return iterator{}; }

		/// @brief Add starting nodes which will also be enumerated.
		void addInputPort(NodePort inputPort);

		/// @brief Add starting nodes which will not be enumerated.
		void addOutputPort(NodePort outputPort);

		/// @brief Add starting nodes which will not be enumerated.
		void addAllOutputPorts(BaseNode *node);
	protected:
		std::vector<OpenNode> m_start;

};

}
