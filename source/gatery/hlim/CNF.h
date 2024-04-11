/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include "RevisitCheck.h"

namespace gtry::hlim {

struct NodePort;
class Subnet;

/**
 * @brief Special case of the Conjunctive Normal Form which is just a conjunction of potentially negated terms (all AND and NOT, no OR).
 */
class Conjunction {
	public:
		/// @see parseInput
		inline static Conjunction fromInput(const NodePort &nodeInput) { Conjunction res; res.parseInput(nodeInput); return res; }
		/// @see parseOutput
		inline static Conjunction fromOutput(const NodePort &nodeOutput) { Conjunction res; res.parseOutput(nodeOutput); return res; }

		/// @brief Parse the logic feeding into the given input port.
		/// @details The logic must not have cycles.
		void parseInput(const NodePort &nodeInput, Subnet *area = nullptr);
		/// @brief Parse the logic feedung into the given input port.
		/// @details The logic must not have cycles.
		void parseOutput(const NodePort &nodeOutput, Subnet *area = nullptr);

		bool isEqualTo(const Conjunction &other) const;
		bool isNegationOf(const Conjunction &other) const;
		bool isSubsetOf(const Conjunction &other) const;

		/// Returns true if this and other can never both be true.
		/// @param checkComparisons Extend this check into the terms to check for mutually exclusive comparisons with e.g. states.
		bool cannotBothBeTrue(const Conjunction &other, bool checkComparisons = false) const;

		/// Returns true if any input feeding into the conjunction is unconnected
		bool isUndefined() const { return m_undefined; }
		/// Returns true if the conjunction contains terms like "A & !A" such that the result is always false
		bool isContradicting() const { return m_contradicting; }

		/// @brief Computes the intersection of the terms, not the intersection
		/// @details I.e. the intersection of A & B & C and B & C & D is B & C.
		/// The intersection of A & B & C and !A & B & C is also B & C
		void intersectTermsWith(const Conjunction &other);

		/// @brief Removes all terms in other from the expression
		/// @details I.e. removing B from A & B & C yields A & C.
		void removeTerms(const Conjunction &other);

		/// @brief Builds a new circuit that computes this conjunction
		/// @details Fails if isUndefined() or isContradicting() which might change in the future
		/// @param targetGroup The node group into which to place the new nodes
		/// @param newNodes If not nullptr, the area to which to add the new nodes.
		/// @param allowUnconnected If the conjunction is constant one, allow outputing an unconnected NodePort.
		/// @return The output port of the final logical AND or {} if the Conjunction is empty (always true)
		NodePort build(NodeGroup &targetGroup, Subnet *newNodes = nullptr, bool allowUnconnected = true) const;

		struct Term {
			/// (non-signal) driver of the raw signal that directly or negated enters the conjunction
			NodePort driver;
			/// Whether or not nonNegatedDriver is negated in the conjunction
			bool negated;
			/// Same as driver or the last equivalent signal node before the signal entered a negation or conjunction
			/// @details This is only kept in case the terms are to be used again to build or drive logic to minimize the amount of
			/// signal nodes that are skipped.
			NodePort conjunctionDriver;

			auto operator<=>(const Term&) const = default;
		};

		const utils::UnstableMap<NodePort, Term> &getTerms() const { return m_terms; }

		auto operator<=>(const Conjunction&) const = default;
	private:
		/// All terms of the conjunction (the parts that are ANDed together), stored as a map for faster lookup by driver
		utils::UnstableMap<NodePort, Term> m_terms;
		bool m_undefined = false;
		bool m_contradicting = false;
};


}