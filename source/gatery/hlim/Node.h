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

#include "NodeIO.h"
#include "NodeVisitor.h"

#include "../utils/StackTrace.h"
#include "../utils/CppTools.h"

#include <vector>
#include <string>
#include <span>

namespace gtry::sim {

class SimulatorCallbacks;

struct DefaultConfig;
template<class Config> class BitVectorState;
using DefaultBitVectorState = BitVectorState<DefaultConfig>;

}

namespace gtry::hlim {

class NodeGroup;
class Clock;
class SignalDelay;
class RevisitCheck;

/**
 * @brief Specifies which signal and clock ports affect a Node's output port's clock domain.
 * @details For there to not be an invalid clock domain crossing with a node's output, all
 * dependent clocks and all dependent inputs must be from the same clock domain.
 */
struct OutputClockRelation {
	std::vector<size_t> dependentInputs;
	std::vector<Clock*> dependentClocks;

	bool isConst() const { return dependentInputs.empty() && dependentClocks.empty(); }
};

/**
 * @brief Specified the clock domain to which a signal belongs.
 * 
 */
struct SignalClockDomain {
	enum Type {
		UNKNOWN,
		CONSTANT,
		CLOCK
	};
	Type type = UNKNOWN;
	Clock *clk = nullptr;
};


class BaseNode : public NodeIO
{
	public:
		BaseNode();
		BaseNode(size_t numInputs, size_t numOutputs);
		virtual ~BaseNode();

		void addRef() { m_refCounter++; }
		void removeRef() { HCL_ASSERT(m_refCounter > 0); m_refCounter--; }
		bool hasRef() const { return m_refCounter > 0; }

		virtual std::string getTypeName() const = 0;
		virtual void assertValidity() const = 0;
		virtual std::string getInputName(size_t idx) const = 0;
		virtual std::string getOutputName(size_t idx) const = 0;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const = 0;

		/// Returns a list of word sizes of all the words of internal state that the node needs for simulation.
		virtual std::vector<size_t> getInternalStateSizes() const { return {}; }
		/// Returns a list of nodes and word indices to refer to the internal state words of other nodes that this node needs to access during simulation.
		virtual std::vector<std::pair<BaseNode*, size_t>> getReferencedInternalStateSizes() const { return {}; }

		virtual void simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const { }
		virtual void simulateResetChange(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort, bool resetHigh) const { }
		virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const { }
		virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const { }
		virtual void simulateClockChange(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort, bool clockValue, bool clockDefined) const { }
		virtual void simulateCommit(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets) const { }

		inline void recordStackTrace() { m_stackTrace.record(10, 1); }
		inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }

		inline void setName(std::string name) { m_name = std::move(name); m_nameInferred = false; }
		inline void setInferredName(std::string name) { m_name = std::move(name); m_nameInferred = true; }
		inline void setComment(std::string comment) { m_comment = std::move(comment); }
		inline const std::string &getName() const { return m_name; }
		inline const std::string &getComment() const { return m_comment; }
		inline bool nameWasInferred() const { return m_nameInferred; }
		inline bool hasGivenName() const { return !m_name.empty() && !m_nameInferred; }

		bool isOrphaned() const;
		virtual bool hasSideEffects() const;
		virtual bool isCombinatorial(size_t port) const;
		virtual bool outputIsConstant(size_t port) const { return false; }

		/// Returns true, if the node's outputs do not change when this input is deasserted (e.g. like a register's enable signal).
		virtual bool inputIsEnable(size_t inputPort) const { return false; }

		/// Returns true if the node has no side effects and given its configuration and inputs, the node is not actually needed.
		virtual bool isNoOp() const { return false; }
		/// Returns true if isNoOp() but also rewires all nodes connected to the output to not use this node.
		virtual bool bypassIfNoOp() { return false; }

		virtual void bypassOutputToInput(size_t outputPort, size_t inputPort) override;

		const NodeGroup *getGroup() const { return m_nodeGroup; }
		NodeGroup *getGroup() { return m_nodeGroup; }

		Circuit *getCircuit();
		const Circuit *getCircuit() const;

		inline const std::vector<Clock*> &getClocks() const { return m_clocks; }

		void moveToGroup(NodeGroup *group);

		virtual void visit(NodeVisitor &visitor) = 0;
		virtual void visit(ConstNodeVisitor &visitor) const = 0;

		void addClock(Clock *clk);
		void attachClock(Clock *clk, size_t clockPort);
		void detachClock(size_t clockPort);

		/// Returns to which clock the ouput signal is related.
		/// @details This function is used to determine the propagation of clock domains through the graph.
		virtual OutputClockRelation getOutputClockRelation(size_t output) const;

		/// Verifies that no unintentional clock domain crossing is happening at this node.
		/// @details Usually, this means that all input clocks must be equal (or constant) and,
		/// if the node itself is clocked, that it is compatible with that clock.
		/// @param inputClocks Clock domain for each input port
		virtual bool checkValidInputClocks(std::span<SignalClockDomain> inputClocks) const;

		/// Returns an id that is unique to this node within the circuit.
		/// @details The id order is preserved when subnets are copied and always reflects creation order.
		inline std::uint64_t getId() const { HCL_ASSERT(m_nodeId != ~0ull); return m_nodeId; }

		void setId(std::uint64_t id, utils::RestrictTo<Circuit>) { m_nodeId = id; }

		/// Returns true, if the signal connected to the input port is coming from a different node group which is also not a child of this node's node group.
		bool inputIsComingThroughParentNodeGroup(size_t inputPort) const;

		/// Attempts to create a reasonable name based on its input names and operations.
		virtual std::string attemptInferOutputName(size_t outputPort) const;

		virtual void estimateSignalDelay(SignalDelay &sigDelay);
		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t& inputPort, size_t& inputBit);

		inline std::uint64_t getRevisitColor(utils::RestrictTo<RevisitCheck>) const { return m_revisitCheckColor; }
		inline void setRevisitColor(std::uint64_t revisitCheckColor, utils::RestrictTo<RevisitCheck>) const { m_revisitCheckColor = revisitCheckColor; }
	protected:
		std::uint64_t m_nodeId = ~0ull;

		bool m_nameInferred = true;
		std::string m_name;
		std::string m_comment;
		utils::StackTrace m_stackTrace;
		NodeGroup *m_nodeGroup = nullptr;
		std::vector<Clock*> m_clocks;

		size_t m_refCounter = 0;
		mutable std::uint64_t m_revisitCheckColor = 0;


		virtual void copyBaseToClone(BaseNode *copy) const;

		void forwardSignalDelay(SignalDelay &sigDelay, unsigned input, unsigned output);
};


template<class FinalType>
class Node : public BaseNode
{
	public:
		Node() : BaseNode() { }
		Node(size_t numInputs, size_t numOutputs) : BaseNode(numInputs, numOutputs) { }

		virtual void visit(NodeVisitor &visitor) override { visitor(*static_cast<FinalType*>(this)); }
		virtual void visit(ConstNodeVisitor &visitor) const override { visitor(*static_cast<const FinalType*>(this)); }


	protected:

};


}
