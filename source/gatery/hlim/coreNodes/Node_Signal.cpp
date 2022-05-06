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
#include "gatery/pch.h"
#include "Node_Signal.h"

#include "../SignalGroup.h"
#include "../SignalDelay.h"

#include "../../utils/Exceptions.h"

#include <string>

namespace gtry::hlim {

void Node_Signal::setConnectionType(const ConnectionType &connectionType)
{
	setOutputConnectionType(0, connectionType);
}

void Node_Signal::connectInput(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = nodePort.node->getOutputConnectionType(nodePort.port);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setConnectionType(paramType);
	}
	NodeIO::connectInput(0, nodePort);
}

void Node_Signal::disconnectInput()
{
	NodeIO::disconnectInput(0);
}

void Node_Signal::moveToSignalGroup(SignalGroup *group)
{
	if (m_signalGroup != nullptr) {
		auto it = std::find(m_signalGroup->m_nodes.begin(), m_signalGroup->m_nodes.end(), this);
		HCL_ASSERT(it != m_signalGroup->m_nodes.end());

		*it = std::move(m_signalGroup->m_nodes.back());
		m_signalGroup->m_nodes.pop_back();
	}

	m_signalGroup = group;
	if (m_signalGroup != nullptr)
		m_signalGroup->m_nodes.push_back(this);
}

std::unique_ptr<BaseNode> Node_Signal::cloneUnconnected() const {
	std::unique_ptr<BaseNode> copy(new Node_Signal());
	copyBaseToClone(copy.get());

	return copy;
}

std::string Node_Signal::attemptInferOutputName(size_t outputPort) const
{
	if (m_name.empty()) return "";

	size_t underscorePos = m_name.find_last_of('_');
	if (underscorePos == std::string::npos)
		return m_name + "_2";

	std::string potentialDigits(m_name.begin() + underscorePos + 1, m_name.end());

	char* end;
	unsigned digits = std::strtoul(potentialDigits.c_str(), &end, 10);
	if(end != potentialDigits.c_str() + potentialDigits.size())
		return m_name + "_2";

	return std::string(m_name.begin(), m_name.begin()+underscorePos+1) + std::to_string(digits+1);
}


void Node_Signal::estimateSignalDelay(SignalDelay &sigDelay)
{
	forwardSignalDelay(sigDelay, 0, 0);
}


void Node_Signal::estimateSignalDelayCriticalInput(SignalDelay &sigDelay, size_t outputPort, size_t outputBit, size_t &inputPort, size_t &inputBit)
{
	inputPort = 0;
	inputBit = outputBit;
}


}
