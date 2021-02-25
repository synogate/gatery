#pragma once

#include <limits>
#include <stddef.h>

namespace hcl::core::hlim {

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