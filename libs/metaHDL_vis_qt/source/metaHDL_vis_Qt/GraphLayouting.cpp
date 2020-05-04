#include "GraphLayouting.h"

#include <metaHDL_core/utils/Range.h>
#include <metaHDL_core/utils/Exceptions.h>
#include <metaHDL_core/utils/Preprocessor.h>

#include <set>
#include <map>

namespace mhdl::vis::layout {

void GraphLayouting::run()
{
    float verticalNodeSpacingPadding = 20;
    float horizontalNodeSpacingPadding = 100;
    
    
    std::map<NodePort, size_t> inputPortToEdge;
    std::map<NodePort, size_t> outputPortToEdge;
    for (auto i : utils::Range(edges.size())) {
        outputPortToEdge[edges[i].src] = i;
        for (auto p : edges[i].dst)
            inputPortToEdge[p] = i;
    }
    
    
    m_nodeLayouts.resize(nodes.size());
    m_edgeLayouts.resize(edges.size());
    
    std::set<size_t> unplacedNodes;
    for (auto i : utils::Range(nodes.size()))
        unplacedNodes.insert(i);
    
    float x = 0.0f;
    
    while (!unplacedNodes.empty()) {
        
        std::map<float, std::vector<size_t>> readyNodes;
        for (auto n : unplacedNodes) {
            float cost = 0.0f;
            for (auto p : utils::Range(nodes[n].relativeInputPortLocations.size())) {
                auto it = inputPortToEdge.find({.node=n, .port=p});
                if (it != inputPortToEdge.end()) {
                    const auto &edge = edges[it->second];
                    if (unplacedNodes.find(edge.src.node) != unplacedNodes.end())
                        cost += edge.weight;
                }
            }
            readyNodes[cost].push_back(n);
        }
        
        MHDL_ASSERT(!readyNodes.empty());
        
        const auto &nodeColumn = readyNodes.begin()->second;
        
        float totalWidth = 0.0f;
        float totalHeight = 0.0f;
        for (auto n : nodeColumn) {
            totalHeight += nodes[n].height;
            totalWidth = std::max(totalWidth, nodes[n].width);
        }
        
        totalHeight += (nodeColumn.size() - 1) * verticalNodeSpacingPadding;
        totalWidth += horizontalNodeSpacingPadding;
        
        
        float y = -totalHeight/2.0f;
        for (auto n : nodeColumn) {
            m_nodeLayouts[n].location.x = x;
            m_nodeLayouts[n].location.y = y + nodes[n].height/2;
            y += nodes[n].height + verticalNodeSpacingPadding;
            
            unplacedNodes.erase(unplacedNodes.find(n));
        }
        
        x += totalWidth;
    }
    
    
    
    for (auto i : utils::Range(edges.size())) {
        m_edgeLayouts[i].lines.resize(edges[i].dst.size());
        for (auto j : utils::Range(edges[i].dst.size())) {
            auto srcNode = edges[i].src.node;
            auto srcPort = edges[i].src.port;
            auto dstNode = edges[i].dst[j].node;
            auto dstPort = edges[i].dst[j].port;
            
            m_edgeLayouts[i].lines[j].from.x = m_nodeLayouts[srcNode].location.x + nodes[srcNode].relativeOutputPortLocations[srcPort].x;
            m_edgeLayouts[i].lines[j].from.y = m_nodeLayouts[srcNode].location.y + nodes[srcNode].relativeOutputPortLocations[srcPort].y;

            m_edgeLayouts[i].lines[j].to.x = m_nodeLayouts[dstNode].location.x + nodes[dstNode].relativeInputPortLocations[dstPort].x;
            m_edgeLayouts[i].lines[j].to.y = m_nodeLayouts[dstNode].location.y + nodes[dstNode].relativeInputPortLocations[dstPort].y;
        }
    }
    
}

}
