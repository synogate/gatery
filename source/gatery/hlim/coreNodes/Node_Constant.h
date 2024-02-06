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
#include <gatery/simulation/BitVectorState.h>

#include <string_view>

namespace gtry::hlim {

	class Node_Constant : public Node<Node_Constant>
	{
	public:
		Node_Constant(bool value);
		Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType connectionType);
		Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType::Type connectionType);

		virtual void simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		const sim::DefaultBitVectorState& getValue() const { return m_Value; }

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;

		virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;

		virtual bool outputIsConstant(size_t port) const override { return true; }
	protected:
		sim::DefaultBitVectorState m_Value;
	};
}
