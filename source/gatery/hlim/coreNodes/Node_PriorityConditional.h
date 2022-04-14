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

namespace gtry::hlim {
	
class Node_PriorityConditional : public Node<Node_PriorityConditional>
{
	public:
		static size_t inputPortDefault() { return 0; }
		static size_t inputPortChoiceCondition(size_t choice) { return 1 + choice*2; }
		static size_t inputPortChoiceValue(size_t choice) { return 1 + choice*2 + 1; }
		
		Node_PriorityConditional() : Node(1, 1) { }
		
		void connectDefault(const NodePort &port);
		void disconnectDefault();

		void connectInput(size_t choice, const NodePort &condition, const NodePort &value);
		void addInput(const NodePort &condition, const NodePort &value);
		void disconnectInput(size_t choice);
		
		virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;		
		
		inline size_t getNumChoices() const { return (getNumInputPorts()-1)/2; }
		
		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
};

}
