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

#include "NodeGroup.h"

#include <set>
#include <map>
#include <string>
#include <memory>
#include <functional>

namespace mhdl {
namespace core {
namespace hlim {

class BaseNode;
class Node_Signal;
class Node_Register;

/*
struct NodeCategorization
{
	std::set<NodePort> drivenByExternal;
	std::set<NodePort> drivingExternal;
	std::set<NodePort> drivenByChild;
	std::set<NodePort> drivingChild;
	std::set<Node_Signal*> internalSignals;
	std::set<Node_Register*> registers;
	std::set<BaseNode*> combinatorial;
	std::set<BaseNode*> unconnected;
	std::set<BaseNode*> unused;
	std::set<NodeGroup*> childGroups;
	
	void parse(NodeGroup *group);
};
*/
}
}
}
