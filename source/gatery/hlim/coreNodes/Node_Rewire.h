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

#include <boost/format.hpp>

#include <vector>

namespace gtry::hlim {

class Node_Rewire : public Node<Node_Rewire>
{
	public:
		struct OutputRange {
			size_t subwidth;
			enum Source {
				INPUT,
				CONST_ZERO,
				CONST_ONE,
				CONST_UNDEFINED,
			};
			Source source;
			size_t inputIdx;
			size_t inputOffset;

			auto operator<=>(const OutputRange &) const = default;
		};

		struct RewireOperation {
			std::vector<OutputRange> ranges;

			bool isBitExtract(size_t& bitIndex) const;

			RewireOperation& addInput(size_t inputIndex, size_t inputOffset, size_t width);
			RewireOperation& addConstant(OutputRange::Source type, size_t width);
		};

		Node_Rewire(size_t numInputs);

		void connectInput(size_t operand, const NodePort &port);
		inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(operand); }

		virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override { }
		virtual std::string getInputName(size_t idx) const override { return (boost::format("in_%d")%idx).str(); }
		virtual std::string getOutputName(size_t idx) const override { return "output"; }

		void setConcat();
		void setInterleave();
		void setExtract(size_t offset, size_t count);
		void setReplaceRange(size_t offset);
		void setPadTo(size_t width, OutputRange::Source padding); // pad input 0
		void setPadTo(size_t width); // pad input 0 with msb

		inline void setOp(RewireOperation rewireOperation) { m_rewireOperation = std::move(rewireOperation); updateConnectionType(); }
		inline const RewireOperation &getOp() const { return m_rewireOperation; }
		
		virtual bool isNoOp() const override;
		virtual bool bypassIfNoOp() override;

		void changeOutputType(ConnectionType outputType);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;

		virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;

		void optimize();

		virtual bool outputIsConstant(size_t port) const override;
	protected:
		ConnectionType m_desiredConnectionType;

		RewireOperation m_rewireOperation;
		void updateConnectionType();

};

}
