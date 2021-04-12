/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include <vector>
#include <functional>

namespace hcl::vis::layout {

struct Location {
    float x, y;
};

struct NodePort {
    size_t node;
    size_t port;
    inline bool operator<(const NodePort &rhs) const { if (node < rhs.node) return true; if (node > rhs.node) return false; return port < rhs.port; }
};
    
struct Node {
    float width;
    float height;
    std::vector<Location> relativeInputPortLocations;
    std::vector<Location> relativeOutputPortLocations;
};

struct Edge {
    float weight = 1.0f;
    
    NodePort src;
    std::vector<NodePort> dst;
};

struct NodeLayout {
    Location location;
};

struct EdgeLayout {
    struct Line {
        Location from, to;
    };
    struct Intersection {
        Location location;
    };
    std::vector<Line> lines;
    std::vector<Intersection> intersections;
};
    
class GraphLayouting {
    public:
        std::vector<Node> nodes;
        std::vector<Edge> edges;
        
        void run(std::function<void(float)> progressCallback = std::function<void(float)>());
        
        inline const std::vector<NodeLayout> &getNodeLayouts() { return m_nodeLayouts; }
        inline const std::vector<EdgeLayout> &getEdgeLayouts() { return m_edgeLayouts; }
    protected:
        std::vector<NodeLayout> m_nodeLayouts;
        std::vector<EdgeLayout> m_edgeLayouts;
};
    
}
