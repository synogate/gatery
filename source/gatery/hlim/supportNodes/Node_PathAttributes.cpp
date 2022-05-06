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

#include "Node_PathAttributes.h"

namespace gtry::hlim {


Node_PathAttributes::Node_PathAttributes() : Node(2, 0)
{
}


void Node_PathAttributes::connectStart(const NodePort &nodePort)
{
	NodeIO::connectInput(0, nodePort);
}

void Node_PathAttributes::disconnectStart()
{
	NodeIO::disconnectInput(0);
}

void Node_PathAttributes::connectEnd(const NodePort &nodePort)
{
	NodeIO::connectInput(1, nodePort);
}

void Node_PathAttributes::disconnectEnd()
{
	NodeIO::disconnectInput(1);
}


std::string Node_PathAttributes::getTypeName() const
{
	return "path_attributes";
}

void Node_PathAttributes::assertValidity() const
{
}

std::string Node_PathAttributes::getInputName(size_t idx) const
{
	if (idx == 0)
		return "start";
	else
		return "end";
}

std::string Node_PathAttributes::getOutputName(size_t idx) const
{
	return "";
}


std::vector<size_t> Node_PathAttributes::getInternalStateSizes() const
{
	return {0};
}

std::unique_ptr<BaseNode> Node_PathAttributes::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> copy(new Node_PathAttributes());
	copyBaseToClone(copy.get());

	((Node_PathAttributes*)copy.get())->m_attributes = m_attributes;

	return copy;
}


}