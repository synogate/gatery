#pragma once

#include <vector>
#include <functional>

namespace mhdl::vis::layout {

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
