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

#include "Attributes.h"

#include "../utils/StackTrace.h"
#include "../utils/ConfigTree.h"
#include "NodeGroupType.h"

#include <vector>
#include <string>
#include <memory>
#include <regex>
#include <variant>
#include <optional>

#include <boost/container/flat_map.hpp>

namespace gtry::hlim {

	class Circuit;
	class BaseNode;
	class Node_Signal;
	class Node_Register;
}


namespace boost::container {
	extern template class flat_map<std::string, size_t>;
}
namespace std {
	extern template class vector<gtry::hlim::BaseNode*>;
	extern template class unique_ptr<gtry::hlim::NodeGroup>;
	//extern template class vector<std::unique_ptr<gtry::hlim::NodeGroup>>;
	extern template class unique_ptr<gtry::hlim::NodeGroupMetaInfo>;
}

namespace gtry::hlim {

	class NodeGroupConfig
	{
	public:
		void load(utils::ConfigTree config);
		void add(std::string_view key, std::string_view filter, utils::ConfigTree setting);
		void reset();
		void finalize();

		std::optional<utils::ConfigTree> operator ()(std::string_view key, std::string_view instancePath) const;

	private:
		struct Setting
		{
			std::string key;
			std::variant<std::string,std::regex> filter;
			utils::ConfigTree value;
		};
		
		std::vector<Setting> m_config;
		std::vector<std::tuple<std::string, std::string, utils::ConfigTree>> m_usedConfig;
	};

	class NodeGroup
	{
	public:
		NodeGroup(Circuit &circuit, NodeGroupType groupType, std::string_view name, NodeGroup* parent);
		virtual ~NodeGroup();

		void recordStackTrace() { m_stackTrace.record(10, 1); }
		const utils::StackTrace& getStackTrace() const { return m_stackTrace; }

		void setInstanceName(std::string name) { m_instanceName = std::move(name); }
		void setComment(std::string comment) { m_comment = std::move(comment); }

		NodeGroup* addChildNodeGroup(NodeGroupType groupType, std::string_view name);
		NodeGroup* findChild(std::string_view name);

		void moveInto(NodeGroup* newParent);

		template<typename Type, typename... Args>
		Type* addSpecialChildNodeGroup(std::string_view name, Args&&... args) {
			m_children.push_back(std::make_unique<Type>(m_circuit, name, std::forward<Args>(args)...));
			m_children.back()->m_parent = this;
			return (Type*)m_children.back().get();
		}

		NodeGroup* getParent() { return m_parent; }
		const NodeGroup* getParent() const { return m_parent; }
		const NodeGroup* getPartition() const;
		const std::string& getName() const { return m_name; }
		const std::string& getInstanceName() const { return m_instanceName; }
		const std::string& getComment() const { return m_comment; }
		const std::vector<BaseNode*>& getNodes() const { return m_nodes; }
		const std::vector<std::unique_ptr<NodeGroup>>& getChildren() const { return m_children; }
		
		const utils::PropertyTree& usedSettings() const { return m_usedSettings; }
		const utils::PropertyTree& properties() const { return m_properties; }
		utils::PropertyTree& properties() { return m_properties; }

		bool isChildOf(const NodeGroup* other) const;
		bool isEmpty(bool recursive) const;

		std::string instancePath() const;

		utils::ConfigTree config(std::string_view attribute);

		inline void setGroupType(NodeGroupType groupType) { m_groupType = groupType; }
		inline NodeGroupType getGroupType() const { return m_groupType; }

		template<typename MetaType, typename... Args>
		MetaType *createMetaInfo(Args&&... args);
		void setMetaInfo(std::unique_ptr<NodeGroupMetaInfo> metaInfo);

		void dropMetaInfo() { m_metaInfo.reset(); }

		NodeGroupMetaInfo* getMetaInfo() { return m_metaInfo.get(); }

		static void configTree(utils::ConfigTree config);

		/**
		 * @brief An alternative way to programatically add settings to the config tree outside of YAML loading. 
		 * @param instanceFilter Path to the instance to apply the setting to. You can use * to match substings and / to seperate instances. A leading / marks an absolute path. "SipHashRound*" would match all sip hash round instances. 
					Note that the area name is not the instance name. All instances have a running index suffix.
		 * @param value The value currently gets converted to a ConfigTree node. That can be interpreted as anything including enums and trees by the Area.
		 * @warning This function does not do anything if gatery is not compiled with YamlCpp.
		 */
		static void configTree(std::string_view instanceFilter, std::string_view attribute, std::string_view value);
		static void configTreeReset();

		inline Circuit &getCircuit() { return m_circuit; }

		/// Returns an id that is unique to this group within the circuit.
		std::uint64_t getId() const { HCL_ASSERT(m_id != ~0ull); return m_id; }

		void setPartition(bool value) { m_isPartition = value; }
		bool isPartition() const { return m_isPartition; }
		void useComponentInstantiation(bool b) { m_useComponentInstantiation = b; }
		bool useComponentInstantiation() const { return m_useComponentInstantiation; }

		inline GroupAttributes &groupAttributes() { return m_groupAttributes; }
		inline const GroupAttributes &groupAttributes() const { return m_groupAttributes; }
	protected:
		Circuit &m_circuit;
		std::uint64_t m_id = ~0ull;
		std::string m_name;
		std::string m_instanceName;
		std::string m_comment;
		NodeGroupType m_groupType;
		utils::PropertyTree m_properties;
		utils::PropertyTree m_usedSettings;
		GroupAttributes m_groupAttributes;

		boost::container::flat_map<std::string, size_t> m_childInstanceCounter;

		std::vector<BaseNode*> m_nodes;
		std::vector<std::unique_ptr<NodeGroup>> m_children;
		NodeGroup* m_parent = nullptr;

		utils::StackTrace m_stackTrace;

		std::unique_ptr<NodeGroupMetaInfo> m_metaInfo;

		friend class BaseNode;
		bool m_isPartition = false;
		bool m_useComponentInstantiation = false;

	private:
		static NodeGroupConfig ms_config;
	};

	template<typename MetaType, typename... Args>
	MetaType *NodeGroup::createMetaInfo(Args&&... args)
	{
		setMetaInfo(std::make_unique<MetaType>(std::forward<Args>(args)...));
		return (MetaType*)m_metaInfo.get();
	}
}


