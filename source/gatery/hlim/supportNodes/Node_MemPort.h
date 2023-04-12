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

class Node_Memory;

class Node_MemPort : public Node<Node_MemPort>
{
	public:
		enum class Inputs {
			enable,
			wrEnable,
			address,
			wrData,
			wrWordEnable,
			orderAfter,
			memoryReadDependency,
			count
		};

		enum class Outputs {
			rdData,
			orderBefore,
			memoryWriteDependency,
			count
		};

		enum class Internal {
			wrData,
			address,
			wrEnable,
			count
		};
		enum class RefInternal {
			memory = (unsigned)Internal::count,
			prevWritePorts = (unsigned)Internal::count+1,
		};


		Node_MemPort(std::size_t bitWidth);

		void changeBitWidth(std::size_t bitWidth);

		void connectMemory(Node_Memory *memory);
		void disconnectMemory();

		size_t getExpectedAddressBits() const;

		Node_Memory *getMemory() const;
		void connectEnable(const NodePort &output);
		void connectWrEnable(const NodePort &output);
		void connectAddress(const NodePort &output);
		void connectWrData(const NodePort &output);
		void connectWrWordEnable(const NodePort &output);
		void orderAfter(Node_MemPort *port);
		bool isOrderedAfter(Node_MemPort *port) const;
		bool isOrderedBefore(Node_MemPort *port) const;

		void setClock(Clock* clk);

		virtual bool isCombinatorial(size_t port) const override { return port == (size_t) Outputs::rdData; }

		bool isReadPort() const;
		bool isWritePort() const;

		virtual void simulatePowerOn(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
		virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
		virtual void simulateAdvance(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets, size_t clockPort) const override;

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		inline size_t getBitWidth() const { return m_bitWidth; }

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;
		virtual std::vector<std::pair<BaseNode*, size_t>> getReferencedInternalStateSizes() const override;

		virtual void estimateSignalDelay(SignalDelay &sigDelay) override;
		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;

		virtual OutputClockRelation getOutputClockRelation(size_t output) const override;
	protected:
		friend class Node_Memory;
		std::size_t m_bitWidth;

		std::vector<Node_MemPort*> getPrevWritePorts() const;
};

}
