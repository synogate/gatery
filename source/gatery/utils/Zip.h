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
#include <tuple>

namespace gtry::utils {
	


template<class ... Containers>
class ZipIterator {
	public:
		using iterator_category = std::forward_iterator_tag;	

		ZipIterator() { }
		ZipIterator(const ZipIterator &other) : m_iterators(other.m_iterators) { }
		ZipIterator(typename Containers::iterator ... iters) : m_iterators(iters...) { }
		ZipIterator(const std::tuple<typename Containers::iterator ...> &iters) : m_iterators(iters) { }

		ZipIterator &operator++() { 
			std::apply([](auto& ...iterators) {
				(++iterators,...);
			}, m_iterators);
			return *this; 
		}

		bool operator==(const ZipIterator &rhs) const { return m_iterators == rhs.m_iterators; }
		bool operator!=(const ZipIterator &rhs) const { return m_iterators != rhs.m_iterators; }
		std::tuple<typename Containers::iterator::value_type&...> operator*() { 
			return std::apply([](auto& ...iterators) {
				return std::make_tuple(std::ref(*iterators)...);
			}, m_iterators);
		}
	protected:
		std::tuple<typename Containers::iterator ...> m_iterators;
};

template<class ... Containers>
class Zip
{
	public:
		Zip(Containers& ... container) : m_containers(container...) { }
		
		typedef ZipIterator<Containers...> iterator;

		iterator begin() { 
			return iterator(std::apply([](auto& ... containers) {
				return std::make_tuple(containers.begin()...); 
			}, m_containers));
		}
		iterator end() { 
			return iterator(std::apply([](auto& ... containers) {
				return std::make_tuple(containers.end()...); 
			}, m_containers));
		}
	protected:
		std::tuple<Containers&...> m_containers;
};

}
