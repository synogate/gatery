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
#include "SignalGroup.h"

#include "coreNodes/Node_Signal.h"

namespace gtry::hlim {

	
SignalGroup::SignalGroup(GroupType groupType) : m_groupType(groupType)
{
}
	
SignalGroup::~SignalGroup()
{
	while (!m_nodes.empty())
		m_nodes.back()->moveToSignalGroup(nullptr);
}


SignalGroup *SignalGroup::addChildSignalGroup(GroupType groupType)
{
	m_children.push_back(std::make_unique<SignalGroup>(groupType));
	m_children.back()->m_parent = this;
	return m_children.back().get();
}



bool SignalGroup::isChildOf(const SignalGroup *other) const
{
	const SignalGroup *parent = getParent();
	while (parent != nullptr) {
		if (parent == other)
			return true;
		parent = parent->getParent();
	}
	return false;
}



}
