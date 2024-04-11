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
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Compare.h"

#include "CNF.h"
#include "GraphTools.h"
#include "Subnet.h"

#include <vector>

namespace gtry::hlim {
	void Conjunction::parseInput(const NodePort &nodeInput, Subnet *area) {
		if (area)
			area->add(nodeInput.node);
		parseOutput(nodeInput.node->getDriver(nodeInput.port), area);
	}

	void Conjunction::parseOutput(const NodePort &nodeOutput, Subnet *area) {
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
			bool canDescendIntoAnd = true;
			NodePort lastLogicDriver;
		};

		std::vector<TraceInfo> stack;
		stack.push_back({
			.signal = nodeOutput,
			.negated = false,
			.canDescendIntoAnd = true,
			.lastLogicDriver = nodeOutput,
		});

		utils::UnstableMap<NodePort, bool> alreadyVisited;
		while (!stack.empty()) {
			auto top = stack.back();
			stack.pop_back();

			if (area)
				area->add(top.signal.node);

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
				bool doAddAsTerm = true;
				if (Node_Constant *constNode = dynamic_cast<Node_Constant*>(top.signal.node)) {
					const auto &value = constNode->getValue();
					HCL_ASSERT(value.size() == 1);
					if (value.get(sim::DefaultConfig::DEFINED, 0)) {
						if (value.get(sim::DefaultConfig::VALUE, 0) ^ top.negated) {
							// The term is ANDed with a constant one, we can just ignore this
							doAddAsTerm = false;
						} else {
							// The term is ANDed with a constant zero. We can ignore this, 
							// but this turns the entire expression FALSE
							doAddAsTerm = false;
							m_contradicting = true;
						}
					}
				} else
				if (Node_Logic *logicNode = dynamic_cast<Node_Logic*>(top.signal.node)) {
					if (logicNode->getOp() == Node_Logic::NOT) {
						stack.push_back({
							.signal = logicNode->getDriver(0), 
							.negated = !top.negated,
							.canDescendIntoAnd = top.negated, // if we have ~(a & b) then this is ~a | ~b, thus after a negation we can't descend into ANDs and add them as terms to our conjunction.
							.lastLogicDriver = logicNode->getDriver(0),
						});
						doAddAsTerm = false;
					} else
					if (top.canDescendIntoAnd && logicNode->getOp() == Node_Logic::AND) {
						for (auto j : utils::Range(logicNode->getNumInputPorts()))
							stack.push_back({
								.signal = logicNode->getDriver(j), 
								.negated = top.negated,
								.lastLogicDriver = logicNode->getDriver(j),
							});
						doAddAsTerm = false;
					}
				} else
				if (dynamic_cast<Node_Signal*>(top.signal.node)) {
					stack.push_back({
						.signal = top.signal.node->getDriver(0),
						.negated = top.negated,
						.lastLogicDriver = top.lastLogicDriver,
					});
					doAddAsTerm = false;
				}
				
				if (doAddAsTerm) {
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
		if (m_contradicting || other.m_contradicting) 
			return m_contradicting && other.m_contradicting;

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
		if (m_contradicting)
			return !other.m_contradicting && other.m_terms.empty();
		if (other.m_contradicting)
			return !m_contradicting && m_terms.empty();

		if (m_terms.size() != other.m_terms.size()) return false;
		if (m_terms.size() != 1) return false; // Requires Demorgan otherwise
		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end()) return false;
			if (it->second.negated == pair.second.negated) return false;
		}
		return m_terms.size() > 0;
	}

	bool Conjunction::isSubsetOf(const Conjunction &other) const {
		if (m_undefined || other.m_undefined) return false;
		if (m_contradicting || other.m_contradicting) return false;

		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it == other.m_terms.end()) return false;
			if (it->second.negated != pair.second.negated) return false;
		}
		return true;
	}

	bool Conjunction::cannotBothBeTrue(const Conjunction &other, bool checkComparisons) const
	{
		if (m_undefined || other.m_undefined)
			return false;
		if (m_contradicting || other.m_contradicting) 
			return true;

		for (const auto &pair : m_terms.anyOrder()) {
			auto it = other.m_terms.find(pair.second.driver);
			if (it != other.m_terms.end()) {
				if (it->second.negated != pair.second.negated)
					return true;

				if (checkComparisons && !it->second.negated && !pair.second.negated) {
					auto cmp1 = isComparisonWithConstant(pair.second.driver);
					if (!cmp1) continue;

					auto cmp2 = isComparisonWithConstant(it->second.driver);
					if (!cmp2) continue;

					if (cmp1->second != cmp2->second) continue;

					if (sim::allDefined(cmp1->first->getValue()) && 
						sim::allDefined(cmp1->first->getValue()) &&
						cmp1->first->getValue().size() == cmp2->first->getValue().size() &&
						cmp1->first->getValue() != cmp2->first->getValue())
						return true;
				}
			}
		}
		return false;
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

	NodePort Conjunction::build(NodeGroup &targetGroup, Subnet *newNodes, bool allowUnconnected) const
	{
		HCL_ASSERT_HINT(!m_undefined, "Can not build undefined conjunction");
		HCL_ASSERT_HINT(!m_contradicting, "Can not build contradicting conjunction");

		Circuit &circuit = targetGroup.getCircuit();

		if (m_terms.empty()) {
			if (allowUnconnected)
				return {};

			auto *oneNode = circuit.createNode<Node_Constant>(sim::parseBit(true), hlim::ConnectionType::BOOL);
			oneNode->moveToGroup(&targetGroup);
			oneNode->recordStackTrace();
			if (newNodes) newNodes->add(oneNode);

			return { .node = oneNode, .port = 0ull };
		}

		// Sort elements to make algorithm stable wrt. reruns
		std::vector<NodePort> sortedTerms;
		sortedTerms.reserve(m_terms.size());
		for (const auto &pair : m_terms.anyOrder())
			sortedTerms.push_back(pair.first);

		std::sort(sortedTerms.begin(), sortedTerms.end(), utils::StableCompare<NodePort>());


		// Replace signals in sortedTerms with either the "conjunctionDriver", or the negation thereof
		for (auto &np : sortedTerms) {
			auto it = m_terms.find(np);
			if (it->second.negated) {
				auto *negationNode = circuit.createNode<Node_Logic>(Node_Logic::NOT);
				negationNode->moveToGroup(&targetGroup);
				negationNode->recordStackTrace();
				negationNode->connectInput(0, it->second.conjunctionDriver);
				if (newNodes) newNodes->add(negationNode);
				np = {.node = negationNode, .port = 0ull};
			} else 
				np = it->second.conjunctionDriver;
		}

		// Build a chain of AND nodes
		/// @todo: change once logic nodes support arbitrarily many inputs
		NodePort last = sortedTerms.front();
		for (auto i : utils::Range<size_t>(1, sortedTerms.size())) {
			auto *andNode = circuit.createNode<Node_Logic>(Node_Logic::AND);
			andNode->moveToGroup(&targetGroup);
			andNode->recordStackTrace();
			andNode->connectInput(0, last);
			andNode->connectInput(1, sortedTerms[i]);
			if (newNodes) newNodes->add(andNode);
			last = {.node = andNode, .port = 0ull};
		}

		return last;
	}


}
