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

#include "NodePort.h"
#include "NodeGroup.h"
#include "Circuit.h"
#include "coreNodes/Node_Logic.h"
#include "coreNodes/Node_Signal.h"

#include "CNF.h"
#include "Subnet.h"

#include <vector>

namespace gtry::hlim {
	void Conjunction::parseInput(const NodePort &nodeInput) {
		parseOutput(nodeInput.node->getDriver(nodeInput.port));
	}

	void Conjunction::parseOutput(const NodePort &nodeOutput) {
		m_terms.clear();
		m_undefined = false;
		m_contradicting = false;

		if (nodeOutput.node == nullptr) {
			m_undefined = true;
			return;
		}

		struct TraceInfo {
			NodePort signal;
			bool negated;
			NodePort lastLogicDriver;
		};

		std::vector<TraceInfo> stack;
		stack.push_back({
			.signal = nodeOutput,
			.negated = false,
			.lastLogicDriver = nodeOutput,
		});

		utils::UnstableMap<NodePort, bool> alreadyVisited;
		while (!stack.empty()) {
			auto top = stack.back();
			stack.pop_back();

			auto it = alreadyVisited.find(top.signal);
			if (it != alreadyVisited.end()) {
				if (it->second != top.negated)
					m_contradicting = true;
				continue;
			}
			alreadyVisited[top.signal] = top.negated;

			if (top.signal.node == nullptr)
				m_undefined = true;
			else {
				bool isRegularNode = true;
				if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(top.signal.node)) {
					if (logicNode->getOp() == Node_Logic::NOT) {
						stack.push_back({
							.signal = logicNode->getNonSignalDriver(0), 
							.negated = !top.negated,
							.lastLogicDriver = logicNode->getDriver(0),
						});
						isRegularNode = false;
					} else
					if (logicNode->getOp() == Node_Logic::AND) {
						for (auto j : utils::Range(logicNode->getNumInputPorts()))
							stack.push_back({
								.signal = logicNode->getNonSignalDriver(j), 
								.negated = top.negated,
								.lastLogicDriver = logicNode->getDriver(j),
							});
						isRegularNode = false;
					}
				} else
				if (dynamic_cast<Node_Signal*>(top.signal.node)) {
					stack.push_back({
						.signal = top.signal.node->getNonSignalDriver(0),
						.negated = top.negated,
						.lastLogicDriver = top.lastLogicDriver,
					});
					isRegularNode = false;
				}
				
				if (isRegularNode) {
					auto it = m_terms.find(top.signal);
					if (it != m_terms.end())
						m_contradicting |= it->second.negated != top.negated;
					else
						m_terms[top.signal] = {
							.driver = top.signal,
							.negated = top.negated,
							.conjunctionDriver = top.lastLogicDriver,
						};
				}
			}
		}
	}

	bool Conjunction::isEqualTo(const Conjunction &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return true;

		if (m_terms.size() != other.m_terms.size()) return false;
		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end()) return false;
			if (it->second.negated != pair.second.negated) return false;
		}
		return true;
	}

	bool Conjunction::isNegationOf(const Conjunction &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return false;

		if (m_terms.size() != other.m_terms.size()) return false;
		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end()) return false;
			if (it->second.negated == pair.second.negated) return false;
		}
		return true;
	}

	bool Conjunction::isSubsetOf(const Conjunction &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting && other.m_contradicting) return false;

		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end()) return false;
			if (it->second.negated != pair.second.negated) return false;
		}
		return true;
	}

	void Conjunction::intersectTermsWith(const Conjunction &other)
	{
		std::vector<NodePort> signalsToRemove;
		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end() || it->second.negated != pair.second.negated)
				signalsToRemove.push_back(pair.second.driver);
		}
		for (auto &np : signalsToRemove)
			m_terms.erase(np);
	}

	void Conjunction::removeTerms(const Conjunction &other)
	{
		for (const auto &pair : other.m_terms.anyOrder()) {
			auto it = m_terms.find(pair.second.driver);
			HCL_ASSERT_HINT(it != m_terms.end(), "Conjunction::removeTerms(other) requires that other is a subset");
			HCL_ASSERT_HINT(it->second.negated == pair.second.negated, "Conjunction::removeTerms(other) requires that other is a subset");
			m_terms.erase(it);
		}
	}

	NodePort Conjunction::build(NodeGroup &targetGroup, Subnet *newNodes) const
	{
		HCL_ASSERT_HINT(!m_undefined, "Can not build undefined conjunction");
		HCL_ASSERT_HINT(!m_contradicting, "Can not build contradicting conjunction");

		if (m_terms.empty())
			return {};

		// Sort elements to make algorithm stable wrt. reruns
		std::vector<NodePort> sortedTerms;
		sortedTerms.reserve(m_terms.size());
		for (const auto &pair : m_terms.anyOrder())
			sortedTerms.push_back(pair.first);

		std::sort(sortedTerms.begin(), sortedTerms.end(), utils::StableCompare<NodePort>());


		Circuit &circuit = targetGroup.getCircuit();

		// Replace signals in sortedTerms with either the "conjunctionDriver", or the negation thereof
		for (auto &np : sortedTerms) {
			auto it = m_terms.find(np);
			if (it->second.negated) {
				auto *negationNode = circuit.createNode<Node_Logic>(Node_Logic::NOT);
				negationNode->recordStackTrace();
				negationNode->connectInput(0, it->second.conjunctionDriver);
				np = {.node = negationNode, .port = 0ull};
			} else 
				np = it->second.conjunctionDriver;
		}

		// Build a chain of AND nodes
		/// @todo: change once logic nodes support arbitrarily many inputs
		NodePort last = sortedTerms.front();
		for (auto i : utils::Range<size_t>(1, sortedTerms.size())) {
			auto *andNode = circuit.createNode<Node_Logic>(Node_Logic::AND);
			andNode->recordStackTrace();
			andNode->connectInput(0, last);
			andNode->connectInput(1, sortedTerms[i]);
			last = {.node = andNode, .port = 0ull};
		}

		return last;
	}


}