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
#include "NodeGroup.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"

#include <boost/format.hpp>

#include <map>
#include <vector>

namespace gtry::hlim 
{
	utils::ConfigTree NodeGroup::ms_config;

	NodeGroup::NodeGroup(GroupType groupType) : m_groupType(groupType)
	{
	}

	NodeGroup::~NodeGroup()
	{
		while (!m_nodes.empty())
			m_nodes.front()->moveToGroup(nullptr);
	}

	void NodeGroup::setName(std::string name)
	{
		if (name != m_name)
		{
			if (m_parent)
			{
				size_t index = m_parent->m_childInstanceCounter[name]++;
				setInstanceName("i_" + name + "_" + std::to_string(index));
			}
			else if (m_instanceName.empty())
			{
				setInstanceName(name);
			}

			m_properties["name"] = name;
			m_name = move(name);
		}
	}

	NodeGroup* NodeGroup::addChildNodeGroup(GroupType groupType)
	{
		auto& child = *m_children.emplace_back(std::make_unique<NodeGroup>(groupType));
		child.m_parent = this;
		return &child;
	}

	NodeGroup* NodeGroup::findChild(std::string_view name)
	{
		for (auto& child : m_children)
			if (child->getName() == name)
				return child.get();

		return nullptr;
	}

	void NodeGroup::moveInto(NodeGroup* newParent)
	{
		if (m_parent == newParent)
			return;

		HCL_ASSERT(m_parent && newParent);
		size_t parentIdx = ~0ull;
		for (auto i : utils::Range(m_parent->m_children.size()))
			if (m_parent->m_children[i].get() == this) {
				parentIdx = i;
				break;
			}

		newParent->m_children.push_back(std::move(m_parent->m_children[parentIdx]));
		m_parent->m_children[parentIdx] = std::move(m_parent->m_children.back());
		m_parent->m_children.pop_back();

		size_t index = m_parent->m_childInstanceCounter[m_name]++;
		setInstanceName("i_" + m_name + "_" + std::to_string(index));
	}

	void NodeGroup::buildProperyTree(utils::PropertyTree& out) const
	{
		utils::PropertyTree me = out[m_instanceName];
		me = m_properties;
		
		for (auto& child : m_children)
		{
			if(!child->getInstanceName().empty())
				child->buildProperyTree(me);
		}
	}

	bool NodeGroup::isChildOf(const NodeGroup* other) const
	{
		const NodeGroup* parent = getParent();
		while (parent != nullptr) {
			if (parent == other)
				return true;
			parent = parent->getParent();
		}
		return false;
	}

	bool NodeGroup::isEmpty(bool recursive) const
	{
		if (!m_nodes.empty()) return false;

		if (recursive)
			for (auto& c : m_children)
				if (!c->isEmpty(recursive))
					return false;

		return true;
	}

	std::string NodeGroup::instancePath() const
	{
		std::string ret = m_instanceName;
		for (NodeGroup* group = m_parent; group; group = group->m_parent)
			if(group->m_parent) // skip root instance
				ret = group->m_instanceName + '/' + ret;
		return ret;
	}

	utils::ConfigTree NodeGroup::instanceConfig() const
	{
		if (!m_parent)
			return ms_config;
		
		auto&& path = instancePath();
		utils::ConfigTree config = ms_config[path];
		config.addRecorder(m_properties);
		return std::move(config);
	}

	void NodeGroup::configTree(utils::ConfigTree config)
	{
		ms_config = std::move(config);
	}
}
