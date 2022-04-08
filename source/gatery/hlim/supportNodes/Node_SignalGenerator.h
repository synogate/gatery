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


class Node_SignalGenerator : public Node<Node_SignalGenerator>
{
	public:
		Node_SignalGenerator(Clock *clk);
		
		virtual void simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
		virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const override;

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;

	protected:
		void setOutputs(const std::vector<ConnectionType> &connections);
		void resetDataDefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const;
		void resetDataUndefinedZero(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const;
		
		virtual void produceSignals(sim::DefaultBitVectorState &state, const size_t *outputOffsets, size_t clockTick) const = 0;
};

}
