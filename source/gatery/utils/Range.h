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
#include <type_traits>

namespace gtry::utils {

template<typename ExposedType, typename = void>
struct RangeIntegralType { using type = ExposedType; };

template<typename ExposedType>
struct RangeIntegralType<ExposedType, std::enable_if_t<std::is_enum<ExposedType>::value>> { using type = std::underlying_type_t<ExposedType>; };

template<typename ExposedType, typename T = void>
using RangeIntegralType_t = typename RangeIntegralType<ExposedType, T>::type;


template<typename ExposedType = size_t, typename Integral = RangeIntegralType_t<ExposedType>>
class Range
{
	public:
		Range(ExposedType beg, ExposedType end) : m_beg(static_cast<Integral>(beg)), m_end(static_cast<Integral>(end)) { }
		Range(ExposedType end) : m_end(static_cast<Integral>(end)) { }
		
		class iterator {
			public:
				using iterator_category = std::forward_iterator_tag;				

				iterator(Integral val) : m_value(val) { }
				
				iterator &operator++() { ++m_value; return *this; }
				iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
				bool operator==(const iterator &rhs) const { return m_value == rhs.m_value; }
				bool operator!=(const iterator &rhs) const { return m_value != rhs.m_value; }
				ExposedType operator*() { return static_cast<ExposedType>(m_value); }
			protected:
				Integral m_value;
		};

		iterator begin() { return iterator(m_beg); }
		iterator end() { return iterator(m_end); }
	protected:
		Integral m_beg = 0;
		Integral m_end = 0;
};
	
}

