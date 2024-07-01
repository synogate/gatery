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
#include "../ClockRational.h"


namespace gtry::sim {
	struct ExtendedConfig;
	using ExtendedBitVectorState = BitVectorState<ExtendedConfig>;
}


namespace gtry::hlim {

	class Node_Pin : public Node<Node_Pin>
	{
		public:

			Node_Pin(bool inputPin, bool outputPin, bool hasOutputEnable);

			void connect(const NodePort &port);
			void connectEnable(const NodePort &port);
			inline void disconnect() { NodeIO::disconnectInput(0); }
			inline void disconnectEnable() { NodeIO::disconnectInput(1); }

			void setBool();
			void setWidth(size_t width);

			bool isInputPin() const { return m_isInputPin; }
			bool isOutputPin() const { return m_isOutputPin; }
			bool isBiDirectional() const { return m_isInputPin && m_isOutputPin; }

			inline const ConnectionType &getConnectionType() const { return m_connectionType; }

			virtual bool hasSideEffects() const override { return true; }
			virtual std::vector<size_t> getInternalStateSizes() const override;

			bool setState(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const sim::ExtendedBitVectorState &newState);
			virtual void simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
			virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

			virtual std::string getTypeName() const override;
			virtual void assertValidity() const override;
			virtual std::string getInputName(size_t idx) const override;
			virtual std::string getOutputName(size_t idx) const override;

			virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

			virtual std::string attemptInferOutputName(size_t outputPort) const override;

			/// In case of an input pin, sets the clock to determine the clock domain to which the input signals are belonging.
			void setClockDomain(Clock *clk);
			/// Despite having a clock for the clock domain, this node is considered combinatorial.
			virtual bool isCombinatorial(size_t port) const override { return true; }

			void setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n");
			void setNormal() { m_differential = false; }

			inline bool isDifferential() const { return m_differential; }
			inline const std::string &getDifferentialPosName() { return m_differentialPosName; }
			inline const std::string &getDifferentialNegName() { return m_differentialNegName; }

			struct PinNodeParameter {
				std::optional<ClockRational> portDelay = {};
				bool delaySpecifiedElsewhere = false;
				/// Whether or not this pin is only placed to attach simulation processes and not to be exported as a top level port map signal.
				bool simulationOnlyPin = false;

				enum class HighImpedanceValue {
					UNDEFINED,
					PULL_UP,
					PULL_DOWN,
				};
				/// The value that is read from this pin if no-one is driving it.
				HighImpedanceValue highImpedanceValue = HighImpedanceValue::UNDEFINED;
			};

			inline PinNodeParameter getPinNodeParameter() const { return m_param; }
			void setPinNodeParameter(PinNodeParameter parameter) { m_param = parameter; }

			virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

			virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;
		protected:
			bool m_isInputPin;
			bool m_isOutputPin;
			bool m_hasOutputEnable;
			ConnectionType m_connectionType;

			bool m_differential = false;			
			std::string m_differentialPosName;
			std::string m_differentialNegName;
			
			PinNodeParameter m_param;
	};

}
