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

#include <iterator>

namespace gtry::utils {

template<class Elements>
class LinkedList;
	
template<class Host>
class LinkedListEntry
{
	public:
		using List = LinkedList<Host>;
		using ListEntry = LinkedListEntry<Host>;
		
		LinkedListEntry(Host &host) : m_host(host) { }
		~LinkedListEntry();
	protected:
		List *m_list = nullptr;
		ListEntry *m_prev = nullptr;
		ListEntry *m_next = nullptr;
		Host &m_host;
		
		friend class LinkedList<Host>::iterator;
		friend class LinkedList<Host>;
};

template<class Elements>
class LinkedList
{
	public:
		using ListEntry = LinkedListEntry<Elements>;
		
		LinkedList() = default;
		LinkedList(const LinkedList &) = delete;
		void operator=(const LinkedList &) = delete;
		
		class iterator {
			public:
				using iterator_category = std::forward_iterator_tag;				

				iterator() { }
				iterator(const iterator &other) : m_current(other.m_current) { }
				iterator(ListEntry &current) : m_current(current) { }
				
				iterator &operator++() { m_current = m_current->m_next; return *this; }
				iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
				bool operator==(const iterator &rhs) const { return m_current == rhs.m_current; }
				bool operator!=(const iterator &rhs) const { return m_current != rhs.m_current; }
				Elements &operator*() { return m_current->m_host; }
			protected:
				ListEntry m_current = nullptr;
		};
		
		iterator begin() { return iterator(m_first); }
		iterator end() { return iterator(); }
		
		size_t size() const { return m_count; }
		bool empty() const { return m_count == 0; }
		
		void deleteAll();
		
		void insertBack(ListEntry &le);
		void remove(ListEntry &le);
		
		
		ListEntry *getFirst() { return m_first; }
		ListEntry *getLast() { return m_last; }
		
		Elements &front() { return m_first->m_host; }
		Elements &back() { return m_last->m_host; }
	protected:
		ListEntry *m_first = nullptr;
		ListEntry *m_last = nullptr;
		size_t m_count = 0;
};

}
