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

#include "Node.h"

#include "../utils/StackTrace.h"
#include "../utils/ConfigTree.h"

#include <vector>
#include <string>
#include <memory>

#include <boost/container/flat_map.hpp>

namespace gtry::hlim {

	class Node_Signal;
	class Node_Register;

	class NodeGroupMetaInfo {
	public:
		virtual ~NodeGroupMetaInfo() = default;
	};


	class NodeGroup
	{
	public:
		enum class GroupType {
			ENTITY = 0x01,
			AREA = 0x02,
			SFU = 0x03,
		};

		NodeGroup(GroupType groupType);
		virtual ~NodeGroup();

		void recordStackTrace() { m_stackTrace.record(10, 1); }
		const utils::StackTrace& getStackTrace() const { return m_stackTrace; }

		void setName(std::string name);
		void setInstanceName(std::string name) { m_instanceName = std::move(name); }
		void setComment(std::string comment) { m_comment = std::move(comment); }

		NodeGroup* addChildNodeGroup(GroupType groupType);
		NodeGroup* findChild(std::string_view name);

		void moveInto(NodeGroup* newParent);

		template<typename Type, typename... Args>
		Type* addSpecialChildNodeGroup(Args&&... args) {
			m_children.push_back(std::make_unique<Type>(std::forward<Args>(args)...));
			m_children.back()->m_parent = this;
			return (Type*)m_children.back().get();
		}

		inline NodeGroup* getParent() { return m_parent; }
		inline const NodeGroup* getParent() const { return m_parent; }
		inline const std::string& getName() const { return m_name; }
		inline const std::string& getInstanceName() const { return m_instanceName; }
		inline const std::string& getComment() const { return m_comment; }
		inline const std::vector<BaseNode*>& getNodes() const { return m_nodes; }
		inline const std::vector<std::unique_ptr<NodeGroup>>& getChildren() const { return m_children; }

		bool isChildOf(const NodeGroup* other) const;
		bool isEmpty(bool recursive) const;

		std::string instancePath() const;
		utils::ConfigTree instanceConfig() const;

		inline GroupType getGroupType() const { return m_groupType; }

		template<typename MetaType, typename... Args>
		void createMetaInfo(Args&&... args);

		NodeGroupMetaInfo* getMetaInfo() { return m_metaInfo.get(); }

		static void configTree(utils::ConfigTree config);
	protected:
		std::string m_name;
		std::string m_instanceName;
		std::string m_comment;
		GroupType m_groupType;

		boost::container::flat_map<std::string, size_t> m_childInstanceCounter;

		std::vector<BaseNode*> m_nodes;
		std::vector<std::unique_ptr<NodeGroup>> m_children;
		NodeGroup* m_parent = nullptr;

		utils::StackTrace m_stackTrace;

		std::unique_ptr<NodeGroupMetaInfo> m_metaInfo;

		friend class BaseNode;

	private:
		static utils::ConfigTree ms_config;
	};

	template<typename MetaType, typename... Args>
	void NodeGroup::createMetaInfo(Args&&... args)
	{
		m_metaInfo.reset(new MetaType(std::forward<Args>(args)...));
	}
}
