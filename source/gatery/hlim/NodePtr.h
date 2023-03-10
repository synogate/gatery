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

#include <type_traits>
#include <compare>

namespace gtry::hlim {

class BaseNode;

class BaseNodePtr
{
	public:
		auto operator <=> (const BaseNodePtr&) const = default;
	protected:
		void addRef();
		void removeRef();
		BaseNode *m_ptr = nullptr;
};

/**
 * @brief Smart pointer for graph nodes that automatically increments and decrements the node's reference counter.
 * @details The pointer default constructs to a nullpointer.
 */
template<class NodeType>
class NodePtr : public BaseNodePtr
{
	public:
		NodePtr() = default;

		NodePtr(const NodePtr<NodeType> &rhs) {
			m_ptr = rhs.m_ptr;
			addRef();
		}
		explicit NodePtr(NodeType *ptr) {
			m_ptr = ptr;
			addRef();
		}
		~NodePtr() {
			removeRef();
		}

		void operator=(const NodePtr<NodeType> &rhs) {
			removeRef();
			m_ptr = rhs.m_ptr;
			addRef();
		}
		void operator=(NodeType *ptr) {
			removeRef();
			m_ptr = ptr;
			addRef();
		}

		auto operator <=> (const NodePtr<NodeType>&) const = default;

		operator NodeType*() const { return (NodeType*) m_ptr; }


		NodeType &operator*() const { return *(NodeType*) m_ptr; }
		NodeType *operator->() const { return (NodeType*) m_ptr; }
		NodeType *get() const { return (NodeType*) m_ptr; }
};

}
