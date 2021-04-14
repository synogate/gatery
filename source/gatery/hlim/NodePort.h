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
#pragma once

#include <limits>
#include <stddef.h>

namespace gtry::hlim {

class BaseNode;
class NodeIO;
class Circuit;

constexpr size_t INV_PORT = std::numeric_limits<std::size_t>::max();

struct NodePort {
    BaseNode *node = nullptr;
    size_t port = INV_PORT;

    inline bool operator==(const NodePort &rhs) const { return node == rhs.node && port == rhs.port; }
    inline bool operator!=(const NodePort &rhs) const { return !operator==(rhs); }
    inline bool operator<(const NodePort &rhs) const { if (node < rhs.node) return true; if (node > rhs.node) return false; return port < rhs.port; }
};


}
