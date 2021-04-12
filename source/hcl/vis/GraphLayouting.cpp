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
#include "GraphLayouting.h"

#include <hcl/utils.h>

#include <set>
#include <map>
#include <queue>

#include <iostream>

namespace hcl::vis::layout {

void GraphLayouting::run(std::function<void(float)> progressCallback)
{
    const float tileScale = 5.0f;

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
        
        HCL_ASSERT(!readyNodes.empty());
        
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
            m_nodeLayouts[n].location.y = std::floor(m_nodeLayouts[n].location.y / tileScale) * tileScale;
            y += nodes[n].height + verticalNodeSpacingPadding;
            
            unplacedNodes.erase(unplacedNodes.find(n));
        }
        
        x += totalWidth;
    }
    
    
    enum { 
        BLOCKED_VERTICAL   = 1,
        BLOCKED_HORIZONTAL = 2,
        BLOCKED            = 3,
    };
    
    
    struct TileIndex {
        int x, y;
        inline bool operator<(const TileIndex &rhs) const { if (x < rhs.x) return true; if (x > rhs.x) return false; return y < rhs.y; }
    };
    
    std::map<TileIndex, std::uint8_t> occupancy;


    for (auto i : utils::Range(nodes.size())) {
        int x_0 = (int)std::floor((m_nodeLayouts[i].location.x - nodes[i].width/2.0f - tileScale/2)/tileScale);
        int x_1 = (int)std::ceil((m_nodeLayouts[i].location.x + nodes[i].width/2.0f + tileScale/2)/tileScale);
        int y_0 = (int)std::floor((m_nodeLayouts[i].location.y - nodes[i].height/2.0f - tileScale/2)/tileScale);
        int y_1 = (int)std::ceil((m_nodeLayouts[i].location.y + nodes[i].height/2.0f + tileScale/2)/tileScale);
        
        for (auto y : utils::Range(y_0, y_1))
            for (auto x : utils::Range(x_0, x_1))
                occupancy[{.x=x, .y=y}] = BLOCKED;
    }


    struct Loc {
        TileIndex tile;
        unsigned dir;
        inline bool operator==(const Loc &rhs) const { return tile.x == rhs.tile.x && tile.y == rhs.tile.y && dir == rhs.dir; }
        inline bool operator<(const Loc &rhs) const { if (tile.x < rhs.tile.x) return true; if (tile.x > rhs.tile.x) return false; if (tile.y < rhs.tile.y) return true; if (tile.y > rhs.tile.y) return false; return dir < rhs.dir; }
    };
    
    struct ScoredLoc : public Loc {
        float cost;
        inline bool operator<(const ScoredLoc &rhs) const { return cost > rhs.cost; }
    };
    
    const float movementCost = 1.0f;
    const float directionChangeCost = 1.0f;
    
    auto manhattanDistance = [](const Loc &lhs, const Loc &rhs) -> float {
        return std::abs(lhs.tile.x - rhs.tile.x) + std::abs(lhs.tile.y - rhs.tile.y);
    };
    
    auto h = [&movementCost, &directionChangeCost](const Loc &lhs, const Loc &rhs) -> float {
        return (std::abs(lhs.tile.x - rhs.tile.x) + std::abs(lhs.tile.y - rhs.tile.y)) * movementCost + std::abs<int>(((lhs.dir + 4 + 2 - rhs.dir) % 4) - 2) * directionChangeCost;
    };
    
    for (auto i : utils::Range(edges.size())) {
        progressCallback((float) i / edges.size());
        
        m_edgeLayouts[i].lines.resize(edges[i].dst.size());
        
        auto srcNode = edges[i].src.node;
        auto srcPort = edges[i].src.port;

        std::set<Loc> possibleStartLocations;
        Loc nodePortStartLocation = {
            .tile = {.x = (int) std::floor((m_nodeLayouts[srcNode].location.x + nodes[srcNode].relativeOutputPortLocations[srcPort].x)/tileScale)+1,
                     .y = (int) std::floor((m_nodeLayouts[srcNode].location.y + nodes[srcNode].relativeOutputPortLocations[srcPort].y)/tileScale)},
            .dir = 0
        };
        possibleStartLocations.insert(nodePortStartLocation);
        
        for (auto j : utils::Range(edges[i].dst.size())) {
            auto dstNode = edges[i].dst[j].node;
            auto dstPort = edges[i].dst[j].port;
            
            Loc destination;
            destination.tile.x = (int) std::floor((m_nodeLayouts[dstNode].location.x + nodes[dstNode].relativeInputPortLocations[dstPort].x)/tileScale)-1;
            destination.tile.y = (int) std::floor((m_nodeLayouts[dstNode].location.y + nodes[dstNode].relativeInputPortLocations[dstPort].y)/tileScale);
            destination.dir = 0;

            //std::cout << "destination " << destination.tile.x << " " << destination.tile.y << "  " << destination.dir << std::endl;
            
            
            std::map<Loc, float> bestScores;
            std::map<Loc, Loc> backLinks;
            std::set<Loc> closedSet;
            
            std::priority_queue<ScoredLoc> openList;
            
            for (auto startLoc : possibleStartLocations) {
                ScoredLoc start;
                start.tile = startLoc.tile;
                start.dir = startLoc.dir;
                start.cost = h(start, destination);
                openList.push(start);
                bestScores[(Loc&)start] = 0.0f;
            }
            
            while (true) {
                HCL_ASSERT(!openList.empty());
                
                ScoredLoc loc = openList.top();
                openList.pop();
                if (closedSet.find(loc) != closedSet.end()) continue;
                closedSet.insert(loc);
                
                loc.cost = bestScores[loc];
                
                //std::cout << "Exploring " << loc.tile.x << " " << loc.tile.y << "  " << loc.dir << std::endl;
                //std::cout << h(loc, destination) << std::endl;
                
                if (loc == destination) {
                    break;
                }
                
#if 1
                ScoredLoc neighbors[3];
                neighbors[0] = loc;
                neighbors[0].dir = (neighbors[0].dir + 3) % 4;
                neighbors[0].cost = loc.cost + directionChangeCost * 
                                    (1.0f + (std::max(0.0f, 5.0f - manhattanDistance(loc, nodePortStartLocation)) + std::max(0.0f, 5.0f - manhattanDistance(loc, destination))) * 1.5f);
                
                neighbors[1] = loc;
                neighbors[1].dir = (neighbors[1].dir + 1) % 4;
                neighbors[1].cost = loc.cost + directionChangeCost * 
                                    (1.0f + (std::max(0.0f, 5.0f - manhattanDistance(loc, nodePortStartLocation)) + std::max(0.0f, 5.0f - manhattanDistance(loc, destination))) * 1.5f);
                
                neighbors[2] = loc;
                switch (neighbors[2].dir) {
                    case 0: neighbors[2].tile.x++; break;
                    case 1: neighbors[2].tile.y++; break;
                    case 2: neighbors[2].tile.x--; break;
                    case 3: neighbors[2].tile.y--; break;
                }
                neighbors[2].cost = loc.cost + movementCost;
                
                for (unsigned j = 0; j < 3; j++) {
                    auto it = occupancy.find(neighbors[j].tile);
                    if (it != occupancy.end()) {
                        if (it->second != 0)
                            neighbors[j].cost += 5.0f;
                        /*
                        if ((neighbors[j].dir == 0 || neighbors[j].dir == 2) && (it->second & BLOCKED_HORIZONTAL))
                            neighbors[j].cost += 10.0f;

                        if ((neighbors[j].dir == 1 || neighbors[j].dir == 3) && (it->second & BLOCKED_VERTICAL))
                            neighbors[j].cost += 10.0f;
                        */
                    }
                    
                    auto costIt = bestScores.find(neighbors[j]);
                    if (costIt == bestScores.end() || costIt->second > neighbors[j].cost) {
                        bestScores[neighbors[j]] = neighbors[j].cost;
                        backLinks[neighbors[j]] = loc;
                        
                        neighbors[j].cost += h(neighbors[j], destination);
                        openList.push(neighbors[j]);
                    }
                }
#else
                ScoredLoc neighbors[4];
                for (unsigned j = 0; j < 4; j++) {
                    neighbors[j] = loc;
                    neighbors[j].cost = loc.cost + 1.0f;
                }
                neighbors[0].tile.x++;
                neighbors[1].tile.x--;
                neighbors[2].tile.y++;
                neighbors[3].tile.y--;
                
                for (unsigned j = 0; j < 4; j++) {
                    auto it = occupancy.find(neighbors[j].tile);
                    if (it != occupancy.end()) {
                        if (it->second != 0)
                            neighbors[j].cost += 5.0f;
/*                        
                        if ((neighbors[j].dir == 0 || neighbors[j].dir == 2) && (it->second & BLOCKED_HORIZONTAL))
                            neighbors[j].cost += 10.0f;

                        if ((neighbors[j].dir == 1 || neighbors[j].dir == 3) && (it->second & BLOCKED_VERTICAL))
                            neighbors[j].cost += 10.0f;
                        */
                    }
                    
                    auto costIt = bestScores.find(neighbors[j]);
                    if (costIt == bestScores.end() || costIt->second > neighbors[j].cost) {
                        bestScores[neighbors[j]] = neighbors[j].cost;
                        backLinks[neighbors[j]] = loc;
                        
                        neighbors[j].cost += h(neighbors[j], destination);
                        openList.push(neighbors[j]);
                    }
                }

#endif
            }
            
            std::vector<Loc> trace, allLocs;
            {
                Loc curr = destination;
                Loc lineStart = curr;
                while (possibleStartLocations.find(curr) == possibleStartLocations.end()) {
                    allLocs.push_back(curr);
                    Loc next = backLinks[curr];
                    
                    if (next.dir != lineStart.dir) {
                        trace.push_back(lineStart);
                        lineStart = next;
                    }
                    
                    occupancy[curr.tile] = BLOCKED;
                    
                    curr = next;
                }
                if (curr.tile.x != lineStart.tile.x || curr.tile.y != lineStart.tile.y)
                    trace.push_back(lineStart);
                trace.push_back(curr);
            }
            
            for (auto l : allLocs)
                possibleStartLocations.insert(l);

            
            auto &lineList = m_edgeLayouts[i].lines;
            if (trace.back() == nodePortStartLocation)
                lineList.push_back({
                    .from = {.x = m_nodeLayouts[srcNode].location.x + nodes[srcNode].relativeOutputPortLocations[srcPort].x,
                            .y = m_nodeLayouts[srcNode].location.y + nodes[srcNode].relativeOutputPortLocations[srcPort].y},
                    .to = {.x = (trace.back().tile.x) * tileScale,
                        .y = (trace.back().tile.y) * tileScale}
                });
            else {
                auto &intersectionList = m_edgeLayouts[i].intersections;
                intersectionList.push_back({
                    .location = {
                        .x = (trace.back().tile.x) * tileScale,
                        .y = (trace.back().tile.y) * tileScale
                }});
            }
            
            for (auto i : utils::Range(trace.size()-1)) {
                lineList.push_back({
                    .from = {.x = (trace[i].tile.x) * tileScale,
                            .y = (trace[i].tile.y) * tileScale},
                    .to = {.x = (trace[i+1].tile.x) * tileScale,
                        .y = (trace[i+1].tile.y) * tileScale}
                });
            }
            
            lineList.push_back({
                .from = {.x = (destination.tile.x) * tileScale,
                         .y = (destination.tile.y) * tileScale},
                .to = {.x = m_nodeLayouts[dstNode].location.x + nodes[dstNode].relativeInputPortLocations[dstPort].x,
                       .y = m_nodeLayouts[dstNode].location.y + nodes[dstNode].relativeInputPortLocations[dstPort].y}
            });
        }
    }
    
}

}
