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

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/ConfigTree.h>

#include "../hlim/NodeGroupType.h"

namespace gtry {

namespace hlim {
	class NodeGroup;
}
	
/**
 * @addtogroup gtry_scopes
 * @{
 */
	
template<class FinalType>
class BaseScope
{
	class Lock {
	public:
		Lock(Lock&& o) : m_ptr(o.m_ptr) { o.m_ptr = nullptr; }
		Lock(const Lock&) = delete;
		Lock(FinalType* ptr) : m_ptr(ptr) {}
		~Lock();

		FinalType* operator -> () { return m_ptr; }
		operator bool() const { return m_ptr != nullptr; }
	private:
		FinalType* m_ptr;
	};

	public:
		BaseScope() { m_parentScope = m_currentScope; m_currentScope = static_cast<FinalType*>(this); }
		~BaseScope() { m_currentScope = m_parentScope; }

		static Lock lock() { Lock ret = m_currentScope; m_currentScope = nullptr; return ret; }
	protected:
		static void unlock(FinalType* scope) { assert(!m_currentScope); m_currentScope = scope; }

		FinalType *m_parentScope;
		static thread_local FinalType *m_currentScope;
};

template<class FinalType>
thread_local FinalType *BaseScope<FinalType>::m_currentScope = nullptr;
	
class GroupScope : public BaseScope<GroupScope>
{
	public:
		using GroupType = hlim::NodeGroupType;
		
		GroupScope(GroupType groupType, std::string_view name);
		GroupScope(hlim::NodeGroup *nodeGroup);
		
		GroupScope &setComment(std::string comment);

		std::string instancePath() const;

		utils::ConfigTree config(std::string_view attribute) const;
		
		const hlim::NodeGroup* nodeGroup() const { return m_nodeGroup; }
		hlim::NodeGroup* nodeGroup() { return m_nodeGroup; }

		utils::PropertyTree operator [] (std::string_view key);

		static GroupScope *get() { return m_currentScope; }
		static hlim::NodeGroup *getCurrentNodeGroup() { return m_currentScope==nullptr?nullptr:m_currentScope->m_nodeGroup; }

		template<typename MetaType, typename... Args>
		MetaType *createMetaInfo(Args&&... args) {
			metaInfo(std::make_unique<MetaType>(std::forward<Args>(args)...));
			return (MetaType*)metaInfo();
		}

		void metaInfo(std::unique_ptr<hlim::NodeGroupMetaInfo> metaInfo);
		hlim::NodeGroupMetaInfo* metaInfo();

		void setPartition(bool value);
		bool isPartition() const;

		void instanceName(std::string name);
		const std::string &instanceName() const;
	protected:
		hlim::NodeGroup *m_nodeGroup;
};

template<class FinalType>
inline BaseScope<FinalType>::Lock::~Lock()
{
	if(m_ptr)
		BaseScope<FinalType>::unlock(m_ptr);
}

/**@}*/

}
