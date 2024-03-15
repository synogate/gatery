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
#include "../../utils/BitFlags.h"

#include <boost/format.hpp>

#include <array>

namespace gtry::hlim {

	class Node_Constant;

	class Node_Register : public Node<Node_Register>
	{
	public:
		enum class Flags {
			ALLOW_RETIMING_FORWARD,
			ALLOW_RETIMING_BACKWARD,
			IS_BOUND_TO_MEMORY,
		};

		enum Input {
			DATA,
			RESET_VALUE,
			ENABLE,
			NUM_INPUTS
		};
		enum Internal {
			INT_DATA,
			INT_ENABLE,
			INT_IN_RESET,
			NUM_INTERNALS
		};

		Node_Register();

		void connectInput(Input input, const NodePort& port);
		inline void disconnectInput(Input input) { NodeIO::disconnectInput(input); }

		virtual void simulatePowerOn(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* outputOffsets) const override;
		virtual void simulateResetChange(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort, bool resetHigh) const override;
		virtual void simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const override;
		virtual void simulateAdvance(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* outputOffsets, size_t clockPort) const override;

		bool overrideOutput(sim::DefaultBitVectorState &state, size_t outputOffset, const sim::DefaultBitVectorState &newState);

		void setClock(Clock* clk);

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;

		virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;

		inline const utils::BitFlags<Flags> &getFlags() const { return m_flags; }
		inline utils::BitFlags<Flags> &getFlags() { return m_flags; }

		bool hasResetValue() const { return getNonSignalDriver((size_t)Input::RESET_VALUE).node != nullptr; }
		bool hasEnable() const { return getNonSignalDriver((size_t)Input::ENABLE).node != nullptr; }

		virtual bool inputIsEnable(size_t inputPort) const override;
	protected:
		utils::BitFlags<Flags> m_flags;

		void writeResetValueTo(sim::DefaultBitVectorState &state, const std::array<size_t, 2> &offsets, size_t width, bool clearDefinedIfUnconnected) const;
	};

}
