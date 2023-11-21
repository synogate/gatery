/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "../Attributes.h"

namespace gtry::hlim {

/**
 * @brief Allows a signal to cross from one clock domain into another.
 * @details The node is placed on intentional CDCs, so that the intended crossing and the two clocks can be verified
 * against the actual clock domains. This node does not set any further attributes such as falsePath.
 */
class Node_CDC : public Node<Node_CDC>
{
	public:
		enum class Clocks {
			INPUT_CLOCK,
			OUTPUT_CLOCK
		};

		Node_CDC();

		virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;		

		virtual bool isCombinatorial(size_t port) const override { return true; }

		void connectInput(const NodePort &nodePort);
		void disconnectInput();

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		virtual OutputClockRelation getOutputClockRelation(size_t output) const override;
		virtual bool checkValidInputClocks(std::span<SignalClockDomain> inputClocks) const override;

		struct CdcNodeParameter {
			std::optional<bool> isGrayCoded = {};
			/// Max skew to set in SDC file as a multiple of the source or destination clock period (whichever is smaller)
			std::optional<double> maxSkew = {};
			/// Max net delay to set in SDC file as a multiple of the destination clock period
			std::optional<double> netDelay = {};
		};

		inline CdcNodeParameter getCdcNodeParameter() const { return m_param; }
		void setCdcNodeParameter(CdcNodeParameter parameter) { m_param = parameter; }

	protected:
		CdcNodeParameter m_param;
};

}
