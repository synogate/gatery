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
#include "Circuit.h"
#include "Node.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"
#include "Circuit.h"

#include <boost/format.hpp>

#include <map>
#include <vector>

namespace boost::container {
	template class flat_map<std::string, size_t>;
}
namespace std {
	template class vector<gtry::hlim::BaseNode*>;
	template class unique_ptr<gtry::hlim::NodeGroup>;
	//template class vector<std::unique_ptr<gtry::hlim::NodeGroup>>;
	template class unique_ptr<gtry::hlim::NodeGroupMetaInfo>;
}

namespace gtry::hlim 
{
	NodeGroupConfig NodeGroup::ms_config;

	NodeGroup::NodeGroup(Circuit &circuit, NodeGroupType groupType, std::string_view name, NodeGroup* parent) : m_circuit(circuit), m_groupType(groupType), m_parent(parent)
	{
		if (m_parent)
		{
			size_t index = m_parent->m_childInstanceCounter[std::string(name)]++;
			setInstanceName(std::string(name) + std::to_string(index));
		}
		else if (m_instanceName.empty())
		{
			setInstanceName(std::string(name));
		}

		std::string fullName = std::string(name);
		auto* it = m_parent;
		while (it != nullptr)
		{
			if (it->isPartition())
			{
				fullName = it->getName() + '_' + fullName;
				break;
			}
			it = it->getParent();
		}

		m_properties["name"] = fullName;
		m_name = std::move(fullName);

		if (auto config = this->config("partition"))
			if (config.as<bool>())
				this->setPartition(true);

		m_id = m_circuit.allocateGroupId({});

		if (auto attrib = this->config("attributes"))
			m_groupAttributes.loadConfig(attrib);
	}

	NodeGroup::~NodeGroup()
	{
		while (!m_nodes.empty())
			m_nodes.front()->moveToGroup(nullptr);
	}

	const NodeGroup* NodeGroup::getPartition() const
	{
		const auto* it = this;
		while (it != nullptr) {
			if (it->isPartition())
				return it;
			it = it->getParent();
		}
		return nullptr;
	}


	NodeGroup* NodeGroup::addChildNodeGroup(NodeGroupType groupType, std::string_view name)
	{
		auto& child = *m_children.emplace_back(std::make_unique<NodeGroup>(m_circuit, groupType, name, this));

		return &child;
	}

	NodeGroup* NodeGroup::findChild(std::string_view name)
	{
		for (auto& child : m_children)
			if (child->getInstanceName() == name)
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
		if (parentIdx+1 != m_parent->m_children.size())
			m_parent->m_children[parentIdx] = std::move(m_parent->m_children.back());
		m_parent->m_children.pop_back();

		m_parent = newParent;

		size_t index = m_parent->m_childInstanceCounter[m_name]++;
		setInstanceName(m_name + std::to_string(index));
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

	void NodeGroup::setMetaInfo(std::unique_ptr<NodeGroupMetaInfo> metaInfo)
	{
		m_metaInfo = std::move(metaInfo);
	}

	std::string NodeGroup::instancePath() const
	{
		std::string ret;

		if (m_parent)
			ret = m_instanceName;

		for (NodeGroup* group = m_parent; group && group->m_parent; group = group->m_parent)
			ret = group->m_instanceName + '/' + ret;

		ret = '/' + ret;
		return ret;
	}

	utils::ConfigTree NodeGroup::config(std::string_view attribute)
	{
		utils::ConfigTree ret;
		if (auto&& setting = ms_config(attribute, instancePath()))
			ret = *setting;
	
		ret.addRecorder(m_properties[attribute]);
		ret.addRecorder(m_usedSettings[attribute]);
		return ret;
	}

	void NodeGroup::configTree(utils::ConfigTree config)
	{
		ms_config.load(config);
	}

	void NodeGroup::configTree(std::string_view instanceFilter, std::string_view attribute, std::string_view value)
	{
#ifdef USE_YAMLCPP
		ms_config.add(attribute, instanceFilter, YAML::Load(std::string{ value }));
		ms_config.finalize();
#endif
	}

	void NodeGroup::configTreeReset()
	{
		ms_config.reset();
	}

	void NodeGroupConfig::load(utils::ConfigTree config)
	{
#ifdef USE_YAMLCPP
		for (auto it = config.mapBegin(); it != config.mapEnd(); ++it)
		{
			auto it_ = *it;
			std::string instance = it.key();
			for (auto itc = it_.mapBegin(); itc != it_.mapEnd(); ++itc)
			{
				add(itc.key(), instance, *itc);
			}
		}
		finalize();
#endif
	}

	void NodeGroupConfig::add(std::string_view key, std::string_view filter, utils::ConfigTree setting)
	{
#ifdef USE_YAMLCPP
		if (filter.find('*') == std::string::npos)
		{
			std::string pattern;
			m_config.emplace_back(Setting{
				.key = std::string{key},
				.filter = std::string{filter} + '/',
				.value = setting
			});
		}
		else
		{
			std::string pattern;
			if (filter.front() == '/')
				pattern += '^';
			else
				pattern += '/';

			for (size_t i = 0; i < filter.size(); ++i)
			{
				if (std::isalnum(filter[i]) || filter[i] == '_' || filter[i] == '/')
					pattern += filter[i];
				else if (filter.substr(i, 2) == "**")
					pattern += ".*";
				else if (filter[i] == '*')
					pattern += "[^/]*";
				else
				{
					HCL_DESIGNCHECK_HINT(false, "found invalid character in config filter pattern");
					pattern += '.';
				}
			}
			pattern += '/';

			m_config.emplace_back(Setting{
				.key = std::string{key},
				.filter = std::regex{pattern, std::regex_constants::optimize},
				.value = setting
			});
		}
#endif
	}

	void NodeGroupConfig::reset()
	{
		m_config.clear();
		m_usedConfig.clear();
	}

	void NodeGroupConfig::finalize()
	{
#ifdef USE_YAMLCPP
		std::ranges::stable_sort(m_config, {}, &Setting::key);
#endif
	}
	
	std::optional<utils::ConfigTree> NodeGroupConfig::operator()(std::string_view key, std::string_view instancePath) const
	{
#ifdef USE_YAMLCPP
		std::string extPath{ instancePath };
		extPath += '/';
#ifdef __clang__
		for (const Setting& elem : m_config) { if (elem.key != key) continue;
#else
		for (const Setting& elem : std::ranges::equal_range(m_config, key, {}, &Setting::key))
		{
#endif
			if (const std::string* path = get_if<std::string>(&elem.filter))
			{
				if (path->front() == '/')
				{
					if (*path == extPath)
						return elem.value;
				}
				else
				{
					if (extPath.ends_with(*path))
						return elem.value;
				}
			}
			else if(std::regex_search(extPath, get<std::regex>(elem.filter)))
			{
				return elem.value;
			}
		}
#endif
		return std::nullopt;
	}
}
