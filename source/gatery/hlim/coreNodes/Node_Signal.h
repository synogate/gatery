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
#include "../ConnectionType.h"

namespace gtry::hlim {

	class SignalGroup;

	class Node_Signal : public Node<Node_Signal>
	{
	public:
		Node_Signal() : Node(1, 1) {  }

		virtual std::string getTypeName() const override { return "Signal"; }
		virtual void assertValidity() const override { }
		virtual std::string getInputName(size_t idx) const override { return "in"; }
		virtual std::string getOutputName(size_t idx) const override { return "out"; }

		void setConnectionType(const ConnectionType &connectionType);

		void connectInput(const NodePort &nodePort);
		void disconnectInput();

		const SignalGroup *getSignalGroup() const { return m_signalGroup; }
		SignalGroup *getSignalGroup() { return m_signalGroup; }

		void moveToSignalGroup(SignalGroup *group);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

		std::string attemptInferOutputName(size_t outputPort) const override;

		virtual void estimateSignalDelay(SignalDelay &sigDelay) override;

		virtual void estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit) override;
	protected:
		SignalGroup *m_signalGroup = nullptr;
	};
}
