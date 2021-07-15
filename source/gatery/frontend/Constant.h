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
#include "BitWidth.h"

#include <gatery/simulation/BitVectorState.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>

#include <string_view>

namespace gtry 
{
    sim::DefaultBitVectorState parseBit(char value);
    sim::DefaultBitVectorState parseBit(bool value);
    sim::DefaultBitVectorState parseBVec(std::string_view);
    sim::DefaultBitVectorState parseBVec(uint64_t value, size_t width);
    
    class BVec;

#if 0 // ConstBVec as class study

    class ConstBVec
    {
    public:
        constexpr ConstBVec(uint64_t value, BitWidth width, std::string_view name = "") : m_intValue(value), m_width(width), m_name(name) {}
        constexpr ConstBVec(BitWidth width, std::string_view name = "") : m_width(width), m_name(name) {}
        constexpr ConstBVec(std::string_view value, std::string_view name = "") : m_stringValue(value), m_name(name) {}

        BVec get() const;
        operator BVec () const;

        sim::DefaultBitVectorState state() const;

    private:
        std::optional<std::string_view> m_stringValue;
        std::optional<uint64_t> m_intValue;
        BitWidth m_width;
        std::string_view m_name;
    };

    std::ostream& operator << (std::ostream&, const ConstBVec&);
#else
    BVec ConstBVec(uint64_t value, BitWidth width, std::string_view name = "");
    BVec ConstBVec(BitWidth width, std::string_view name = ""); // undefined constant
#endif

}

#define GTRY_CONST_BVEC(x, value) \
    struct x { operator BVec () { \
        BVec res = value; \
        res.getNode()->getNonSignalDriver(0).node->setName(#x); \
        return res; \
    } const char *getName() const { return #x; } auto getValue() const { return value; } };

#define GTRY_CONST_BIT(x, value) \
    struct x { operator Bit () { \
        Bit res = value; \
        res.getNode()->getNonSignalDriver(0).node->setName(#x); \
        return res; \
    } const char *getName() const { return #x; } auto getValue() const { return value; } };
