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
#include "StableContainers.h"

#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_MultiDriver.h>
#include <gatery/hlim/NodePort.h>
#include <gatery/hlim/NodeGroup.h>
#include <gatery/hlim/Clock.h>

namespace gtry::utils {


bool StableCompare<hlim::NodePort>::operator()(const hlim::NodePort &lhs, const hlim::NodePort &rhs) const {
	if (lhs.node == nullptr) {
		return rhs.node != nullptr;
	} else {
		if (rhs.node == nullptr) return false;
		if (lhs.node->getId() < rhs.node->getId())
			return true;
		if (lhs.node->getId() > rhs.node->getId())
			return false;
		return lhs.port < rhs.port;
	}
}


bool StableCompare<hlim::RefCtdNodePort>::operator()(const hlim::RefCtdNodePort &lhs, const hlim::RefCtdNodePort &rhs) const {
	if (lhs.node == nullptr) {
		return rhs.node != nullptr;
	} else {
		if (rhs.node == nullptr) return false;
		if (lhs.node->getId() < rhs.node->getId())
			return true;
		if (lhs.node->getId() > rhs.node->getId())
			return false;
		return lhs.port < rhs.port;
	}
}

template<typename Type>
bool stableCompareWithId(const Type* const &lhs, const Type* const &rhs) {
	if (lhs == nullptr)
		return rhs != nullptr;
	if (rhs == nullptr) return false;
	return lhs->getId() < rhs->getId();
}

bool stableCompareNodes(const hlim::BaseNode* const &lhs, const hlim::BaseNode* const &rhs)
{
	return stableCompareWithId(lhs, rhs);
}

bool StableCompare<hlim::Clock*>::operator()(const hlim::Clock* const &lhs, const hlim::Clock* const &rhs) const
{
	return stableCompareWithId(lhs, rhs);
}

bool StableCompare<hlim::NodeGroup*>::operator()(const hlim::NodeGroup* const &lhs, const hlim::NodeGroup* const &rhs) const
{
	return stableCompareWithId(lhs, rhs);
}

bool StableCompare<hlim::Node_Pin*>::operator()(const hlim::Node_Pin* const &lhs, const hlim::Node_Pin* const &rhs) const
{
	return stableCompareWithId(lhs, rhs);
}

bool StableCompare<hlim::Node_MultiDriver*>::operator()(const hlim::Node_MultiDriver* const &lhs, const hlim::Node_MultiDriver* const &rhs) const
{
	return stableCompareWithId(lhs, rhs);
}

}