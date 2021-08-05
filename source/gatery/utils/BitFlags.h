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

template<typename EnumType>
class BitFlags {
	public:
		typedef BitFlags<EnumType> Self;

		Self &operator=(EnumType e) { m_flags = e; return *this; }
		operator EnumType() { return m_flags; }

		Self &insert(EnumType e) { m_flags = EnumType(m_flags | e); return *this; }
		Self &clear(EnumType e) { m_flags = EnumType(m_flags & ~e); return *this; }
		bool containsAnyOf(EnumType e) const { return m_flags & e; }
	protected:
		EnumType m_flags = EnumType(0);
};

    
}

