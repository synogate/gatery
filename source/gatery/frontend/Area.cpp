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
#include "Area.h"

gtry::Area::Area(std::string_view name, bool instantEnter)
{
	auto* parent = GroupScope::getCurrentNodeGroup();
	m_nodeGroup = parent->addChildNodeGroup(hlim::NodeGroup::GroupType::ENTITY, name);
	m_nodeGroup->recordStackTrace();

	if (instantEnter)
		m_inScope.emplace(m_nodeGroup);
}

gtry::GroupScope gtry::Area::enter() const
{
	return { m_nodeGroup };
}

std::pair<gtry::GroupScope, gtry::GroupScope> gtry::Area::enter(std::string_view subName) const
{
	hlim::NodeGroup* sub = m_nodeGroup->addChildNodeGroup(hlim::NodeGroup::GroupType::ENTITY, subName);
	sub->recordStackTrace();

	return { m_nodeGroup, sub };
}
