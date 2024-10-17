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

#include "Node_Attributes.h"

namespace gtry::hlim {


Node_Attributes::Node_Attributes() : Node(1, 1)
{

}

void Node_Attributes::setConnectionType(const ConnectionType &connectionType)
{
	setOutputConnectionType(0, connectionType);
}

void Node_Attributes::connectInput(const NodePort &nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = hlim::getOutputConnectionType(nodePort);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setConnectionType(paramType);
	}

	NodeIO::connectInput(0, nodePort);
}


void Node_Attributes::disconnectInput()
{
	NodeIO::disconnectInput(0);
}


bool Node_Attributes::canBeRetimedOver() const
{
	return !m_attributes.preventRetimingOver();
}


std::string Node_Attributes::getTypeName() const
{
	return "attribute";
}

void Node_Attributes::assertValidity() const
{
}

std::string Node_Attributes::getInputName(size_t idx) const
{
	return "in";
}

std::string Node_Attributes::getOutputName(size_t idx) const
{
	return "";
}


std::vector<size_t> Node_Attributes::getInternalStateSizes() const
{
	return {};
}

std::unique_ptr<BaseNode> Node_Attributes::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_Attributes());
	copyBaseToClone(copy.get());

	((Node_Attributes*)copy.get())->m_attributes = m_attributes;

	return copy;
}

std::string Node_Attributes::attemptInferOutputName(size_t outputPort) const
{
	std::stringstream name;

	auto driver = getDriver(0);
	if (driver.node == nullptr) return "";
	if (inputIsComingThroughParentNodeGroup(0)) return "";
	if (driver.node->getName().empty()) return "";

	return driver.node->getName() + "_attrib";
}




}