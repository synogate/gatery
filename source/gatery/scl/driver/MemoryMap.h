/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */


#include <span>
#include <string>
#include <string_view>
#include <iostream>
#include <map>
#include <vector>
#include <cstdint>

#include "MemoryMapEntry.h"

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {


class MemoryMap;

class MemoryMapEntryHandle {
    public:
		constexpr size_t addr() const { return entry().addr; }
		constexpr size_t width() const { return entry().width; }
		constexpr bool readable() const { return entry().flags & MemoryMapEntry::READABLE; }
		constexpr bool writeable() const { return entry().flags & MemoryMapEntry::WRITEABLE; }
		constexpr const char *name() const { return entry().name; }
		constexpr const char *longDesc() const { return entry().longDesc; }
		constexpr const char *shortDesc() const { return entry().shortDesc; }

        constexpr MemoryMapEntryHandle() { }
        constexpr MemoryMapEntryHandle(std::span<const MemoryMapEntry> allEntries, size_t idx);
        
		constexpr size_t size() const { return entry().childrenCount; }
		constexpr MemoryMapEntryHandle operator[](const std::string_view &name) const;
		constexpr MemoryMapEntryHandle operator[](size_t idx) const;

		class const_iterator {
			public:
				constexpr const_iterator();

				constexpr const_iterator &operator++() { m_childIdx++; return *this; }
				constexpr bool operator==(const const_iterator &rhs) const;
				constexpr MemoryMapEntryHandle operator*() const;
			protected:
				std::span<const MemoryMapEntry> m_allEntries;
				size_t m_parentIdx = ~0ull;
				size_t m_childIdx = ~0ull;

				friend class MemoryMapEntryHandle;
				constexpr const_iterator(std::span<const MemoryMapEntry> allEntries, size_t idx);
		};

		constexpr const_iterator begin() const { return const_iterator(m_allEntries, m_idx); }
		constexpr const_iterator cbegin() const { return const_iterator(m_allEntries, m_idx); }
		constexpr const_iterator end() const { return const_iterator(); }
		constexpr const_iterator cend() const { return const_iterator(); }
    protected:
        std::span<const MemoryMapEntry> m_allEntries;
        size_t m_idx;

		constexpr const MemoryMapEntry &entry() const { return m_allEntries[m_idx]; }
};

class MemoryMap : public MemoryMapEntryHandle {
    public:
        constexpr MemoryMap() { }
        constexpr MemoryMap(std::span<const MemoryMapEntry> allEntries) : MemoryMapEntryHandle(allEntries, 0) { }
};



template<typename T>
concept IsStaticMemoryMapEntryHandle = requires (T t) {
	{ (MemoryMapEntryHandle)t } -> std::convertible_to<MemoryMapEntryHandle>;
};




template<size_t length>
struct TemplateString {
    constexpr TemplateString(const char (&stringLiteral)[length]) {
        std::copy(stringLiteral, stringLiteral + length, value);
    }
    
    char value[length];
};


template<typename Parent, TemplateString Name>
class StaticMemoryMapEntryHandle {
	public:
        constexpr StaticMemoryMapEntryHandle() = default;

		constexpr size_t addr() const { return entry().addr(); }
		constexpr size_t width() const { return entry().width(); }
		constexpr bool readable() const { return entry().readable(); }
		constexpr bool writeable() const { return entry().writeable(); }
		constexpr const char *name() const { return entry().name(); }
		constexpr const char *longDesc() const { return entry().longDesc(); }
		constexpr const char *shortDesc() const { return entry().shortDesc(); }
        
		constexpr size_t size() const { return entry().size(); }
		constexpr operator MemoryMapEntryHandle() const { return entry(); }

        template<TemplateString name>
        constexpr auto get() const { return StaticMemoryMapEntryHandle<decltype(*this), name>{}; }
    protected:
		constexpr MemoryMapEntryHandle entry() const { 
            return ((MemoryMapEntryHandle)Parent{})[Name.value];
        }
};


template<typename T>
class DynamicMemoryMap {
	public:
        static MemoryMap memoryMap;
        constexpr operator MemoryMapEntryHandle() const { return memoryMap; }

        template<TemplateString name>
        constexpr auto get() const { return StaticMemoryMapEntryHandle<decltype(*this), name>{}; }
};

template<typename T>
MemoryMap DynamicMemoryMap<T>::memoryMap;

template<const MemoryMapEntry entries[]>
class StaticMemoryMap {
	public:
        static constexpr MemoryMap memoryMap = MemoryMap(std::span<const MemoryMapEntry>(entries, sizeof(entries) / sizeof(MemoryMapEntry)));
		constexpr operator MemoryMapEntryHandle() const { return memoryMap; }

        template<TemplateString name>
        consteval auto get() const { return StaticMemoryMapEntryHandle<decltype(*this), name>{}; }

		constexpr size_t width() const { return memoryMap.width(); }		
};






//////////////////////////

constexpr MemoryMapEntryHandle::const_iterator::const_iterator()
{

}


constexpr MemoryMapEntryHandle::const_iterator::const_iterator(std::span<const MemoryMapEntry> allEntries, size_t idx) 
			: m_allEntries(allEntries), m_parentIdx(idx), m_childIdx(0)
{
}

constexpr bool MemoryMapEntryHandle::const_iterator::operator==(const const_iterator &rhs) const
{
	if (rhs.m_parentIdx == ~0ull) {
		if (m_parentIdx == ~0ull)
			return true;
		if (m_childIdx >= m_allEntries[m_parentIdx].childrenCount)
			return true;
		return false;
	} else {
		if (m_parentIdx == ~0ull)
			return rhs == *this;
		
		return 
			m_allEntries.data() == rhs.m_allEntries.data() &&
			m_parentIdx == rhs.m_parentIdx &&
			m_childIdx == rhs.m_childIdx;
	}
}

constexpr MemoryMapEntryHandle MemoryMapEntryHandle::const_iterator::operator*() const
{
	return MemoryMapEntryHandle(m_allEntries, m_allEntries[m_parentIdx].childrenStart + m_childIdx);
}




constexpr MemoryMapEntryHandle::MemoryMapEntryHandle(std::span<const MemoryMapEntry> allEntries, size_t idx) : m_allEntries(allEntries), m_idx(idx) { }


constexpr MemoryMapEntryHandle MemoryMapEntryHandle::operator[](const std::string_view &name) const { 
    //static_assert(std::is_constant_evaluated());

    size_t start = m_allEntries[m_idx].childrenStart;
    size_t end = start+m_allEntries[m_idx].childrenCount;

    for (size_t i = start; i < end; i++)
        if (name == m_allEntries[i].name)
            return MemoryMapEntryHandle(m_allEntries, i);

    throw std::range_error("Not found");
}

constexpr MemoryMapEntryHandle MemoryMapEntryHandle::operator[](size_t idx) const { 
	return idx < entry().childrenCount?MemoryMapEntryHandle(m_allEntries, entry().childrenStart + idx) : throw std::range_error("Child index out of range"); 
}


}
