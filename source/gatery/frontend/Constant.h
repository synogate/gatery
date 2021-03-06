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

    BVec ConstBVec(uint64_t value, BitWidth width);
    BVec ConstBVec(BitWidth width); // undefined constant
}

#define GTRY_CONST_BVEC(x, value) \
    struct x { operator BVec () { \
        BVec res = value; \
        res.getNode()->getNonSignalDriver(0).node->setName(#x); \
        return res; \
    } };

#define GTRY_CONST_BIT(x, value) \
    struct x { operator Bit () { \
        Bit res = value; \
        res.getNode()->getNonSignalDriver(0).node->setName(#x); \
        return res; \
    } };
