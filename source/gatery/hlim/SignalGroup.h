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

#include "../utils/StackTrace.h"

#include <vector>
#include <string>
#include <memory>

namespace gtry::hlim {

class Node_Signal;
	
class SignalGroup
{
	public:
		enum class GroupType {
			ARRAY	  = 0x01,
			STRUCT	 = 0x02,
		};
		
		SignalGroup(GroupType groupType);
		~SignalGroup();
		
		inline void recordStackTrace() { m_stackTrace.record(10, 1); }
		inline const utils::StackTrace &getStackTrace() const { return m_stackTrace; }
		
		inline void setName(std::string name) { m_name = std::move(name); }
		inline void setComment(std::string comment) { m_comment = std::move(comment); }
		
		SignalGroup *addChildSignalGroup(GroupType groupType);
		
		inline const SignalGroup *getParent() const { return m_parent; }
		inline const std::string &getName() const { return m_name; }
		inline const std::string &getComment() const { return m_comment; }
		inline const std::vector<Node_Signal*> &getNodes() const { return m_nodes; }
		inline const std::vector<std::unique_ptr<SignalGroup>> &getChildren() const { return m_children; }

		bool isChildOf(const SignalGroup *other) const;
		
		inline GroupType getGroupType() const { return m_groupType; }
	protected:
		std::string m_name;
		std::string m_comment;
		GroupType m_groupType;
		
		std::vector<Node_Signal*> m_nodes;
		std::vector<std::unique_ptr<SignalGroup>> m_children;
		SignalGroup *m_parent = nullptr;
		
		utils::StackTrace m_stackTrace;
		
		friend class Node_Signal;
};

}
