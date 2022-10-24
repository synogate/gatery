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

#include "../Node.h"


namespace gtry::hlim {

/**
 * @brief Defines an export override for a signal
 * @details Overrides allow to specify two separate networks for producing the same output.
 * Both networks can coexist with the simulation using the primary input and the export using the other.
 * In this way, external nodes can forcve macro instantiations while retaining a simulation model at the same time.
 */
class Node_ExportOverride : public Node<Node_ExportOverride>
{
	public:
		enum Inputs {
			SIM_INPUT,
			EXP_INPUT
		};

		Node_ExportOverride();

		void setConnectionType(const ConnectionType &connectionType);

		void connectInput(const NodePort &nodePort);
		void connectOverride(const NodePort &nodePort);
		void disconnectInput();

		virtual void simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const override;

		virtual bool hasSideEffects() const override { return false; }

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;
	protected:
};

}
