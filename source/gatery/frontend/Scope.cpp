#include "Scope.h"
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
#include "Scope.h"
#include "trace.h"

#include <gatery/hlim/NodeGroup.h>

namespace gtry 
{

	GroupScope::GroupScope(hlim::NodeGroupType groupType, std::string_view name) : BaseScope<GroupScope>()
	{
		m_nodeGroup = m_parentScope->m_nodeGroup->addChildNodeGroup(groupType, name);
		m_nodeGroup->recordStackTrace();
	}

	GroupScope::GroupScope(hlim::NodeGroup* nodeGroup) : BaseScope<GroupScope>()
	{
		m_nodeGroup = nodeGroup;
	}

	GroupScope& GroupScope::setComment(std::string comment)
	{
		m_nodeGroup->setComment(std::move(comment));
		return *this;
	}

	std::string GroupScope::instancePath() const
	{
		return m_nodeGroup->instancePath();
	}

	utils::ConfigTree GroupScope::config(std::string_view attribute) const
	{
		return m_nodeGroup->config(attribute);
	}

	utils::PropertyTree GroupScope::operator [] (std::string_view key)
	{
		return m_nodeGroup->properties()[key];
	}

	void GroupScope::metaInfo(std::unique_ptr<hlim::NodeGroupMetaInfo> metaInfo)
	{
		m_nodeGroup->setMetaInfo(std::move(metaInfo));
	}

	hlim::NodeGroupMetaInfo* GroupScope::metaInfo()
	{
		return m_nodeGroup->getMetaInfo();
	}

	void GroupScope::setPartition(bool value) 
	{ 
		m_nodeGroup->setPartition(value); 
	}

	bool GroupScope::isPartition() const
	{
		return m_nodeGroup->isPartition(); 
	}

	void GroupScope::instanceName(std::string name)
	{
		m_nodeGroup->setInstanceName(std::move(name));
	}

	const std::string &GroupScope::instanceName() const
	{
		return m_nodeGroup->getInstanceName();
	}

}
