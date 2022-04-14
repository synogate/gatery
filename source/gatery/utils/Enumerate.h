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
#include <utility>
#include <functional>

namespace gtry::utils {
	
template<class Container>
class Enumerate
{
	public:
		Enumerate(Container &container) : m_container(container) { }
		
		class iterator {
			public:
				using iterator_category = std::forward_iterator_tag;				
				using DeferredIterator = typename Container::iterator;

				iterator() { }
				iterator(const iterator &other) : m_iter(other.m_iter), m_index(other.m_index) { }
				iterator(const DeferredIterator &iter) : m_iter(iter) { }
				
				iterator &operator++() { ++m_iter; ++m_index; return *this; }
				iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
				bool operator==(const iterator &rhs) const { return m_iter == rhs.m_iter; }
				bool operator!=(const iterator &rhs) const { return m_iter != rhs.m_iter; }
				std::pair<size_t, typename DeferredIterator::value_type&> operator*() { return std::make_pair(m_index, std::ref(*m_iter)); }
			protected:
				DeferredIterator m_iter;
				size_t m_index = 0;
		};

		iterator begin() { return iterator(m_container.begin()); }
		iterator end() { return iterator(m_container.end()); }
	protected:
		Container &m_container;
};

template<class Container>
class ConstEnumerate
{
	public:
		ConstEnumerate(const Container &container) : m_container(container) { }
		
		class iterator {
			public:
				using iterator_category = std::forward_iterator_tag;				
				using DeferredIterator = typename Container::const_iterator;

				iterator() { }
				iterator(const iterator &other) : m_iter(other.m_iter), m_index(other.m_index) { }
				iterator(const DeferredIterator &iter) : m_iter(iter) { }
				
				iterator &operator++() { ++m_iter; ++m_index; return *this; }
				iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
				bool operator==(const iterator &rhs) const { return m_iter == rhs.m_iter; }
				bool operator!=(const iterator &rhs) const { return m_iter != rhs.m_iter; }
				std::pair<size_t, const typename DeferredIterator::value_type&> operator*() { return std::make_pair(m_index, std::ref(*m_iter)); }
			protected:
				DeferredIterator m_iter;
				size_t m_index = 0;
		};

		iterator begin() { return iterator(m_container.cbegin()); }
		iterator end() { return iterator(m_container.cend()); }
	protected:
		const Container &m_container;
};

}
