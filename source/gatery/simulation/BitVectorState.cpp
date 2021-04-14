/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "gatery/pch.h"
#include "BitVectorState.h"

namespace hcl::sim {

DefaultBitVectorState createDefaultBitVectorState(std::size_t size, const void *data) {
    BitVectorState<DefaultConfig> state;
    state.resize(size*8);
    state.setRange(DefaultConfig::DEFINED, 0, size*8);
    memcpy(state.data(DefaultConfig::VALUE), data, size);
    return state;
}

}
