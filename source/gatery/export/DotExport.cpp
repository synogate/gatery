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
#include "gatery/pch.h"
#include "DotExport.h"

#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Compare.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/supportNodes/Node_SignalTap.h"

#include "../hlim/Subnet.h"
#include "../hlim/SignalDelay.h"

#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstdlib>

// Adopted from Andy's codebase

namespace gtry {

DotExport::DotExport(std::filesystem::path destination) : m_destination(std::move(destination))
{
}

void DotExport::operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet)
{
    if (m_mergeCombinatoryNodes)
        writeMergedDotFile(circuit, subnet);
    else
        writeDotFile(circuit, subnet, nullptr, nullptr);
}

void DotExport::operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, const hlim::SignalDelay &signalDelays)
{
    writeDotFile(circuit, subnet, nullptr, &signalDelays);
}

void DotExport::operator()(const hlim::Circuit &circuit, hlim::NodeGroup *nodeGroup)
{
    if (m_mergeCombinatoryNodes && nodeGroup == nullptr)
        writeMergedDotFile(circuit, hlim::ConstSubnet::all(circuit));
    else
        writeDotFile(circuit, hlim::ConstSubnet::all(circuit), nodeGroup, nullptr);
}

void DotExport::writeDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, hlim::NodeGroup *nodeGroup, const hlim::SignalDelay *signalDelays)
{
    std::fstream file(m_destination.string().c_str(), std::fstream::out);
    if (!file.is_open())
        throw std::runtime_error("Could not open file!");
    file << "digraph G {" << std::endl;

    std::map<hlim::BaseNode*, unsigned> node2idx;
  //  std::map<hlim::NodeGroup*, unsigned> nodeGroup2idx;


    auto styleNode = [](std::fstream &file, hlim::BaseNode *node) {
        if (dynamic_cast<hlim::Node_Register*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0a0ff\"";
        else if (dynamic_cast<hlim::Node_Constant*>(node))
            file << " shape=\"ellipse\" style=\"filled\" fillcolor=\"#ffa0a0\"";
        else if (dynamic_cast<hlim::Node_Multiplexer*>(node))
            file << " shape=\"diamond\" style=\"filled\" fillcolor=\"#b0b0b0\"";
        else if (dynamic_cast<hlim::Node_Arithmetic*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0ffa0\"";
        else if (dynamic_cast<hlim::Node_Logic*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#ffffa0\"";
        else if (dynamic_cast<hlim::Node_Compare*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#ffd0a0\"";
        else if (dynamic_cast<hlim::Node_Pin*>(node))
            file << " shape=\"house\"";
        else if (dynamic_cast<hlim::Node_SignalTap*>(node))
            file << " shape=\"cds\"";
        else
            if (node->hasRef())
                file << " shape=\"box\" style=\"filled\" fillcolor=\"#eeeeee\"";
            else
                file << " shape=\"box\"";

    };

    {
        unsigned idx = 0;
        unsigned graphIdx = 0;

        std::function<void(const hlim::NodeGroup *nodeGroup)> reccurWalkNodeGroup;
        reccurWalkNodeGroup = [&](const hlim::NodeGroup *nodeGroup) {

            file << "subgraph cluster_" << graphIdx << "{" << std::endl;
          //  nodeGroup2idx[nodeGroup] = graphIdx++;
            graphIdx++;

            file << " label=\"" << nodeGroup->getInstanceName() << "\";" << std::endl;
            switch (nodeGroup->getGroupType()) {
                case hlim::NodeGroup::GroupType::ENTITY:
                    file << " color=blue;" << std::endl;
                break;
                case hlim::NodeGroup::GroupType::AREA:
                    file << " color=black; style=filled; fillcolor=azure; " << std::endl;
                break;
                case hlim::NodeGroup::GroupType::SFU:
                    file << " color=black; style=filled; fillcolor=beige;" << std::endl;
                break;
            }

            for (const auto &subGroup : nodeGroup->getChildren())
                reccurWalkNodeGroup(subGroup.get());

            for (auto *node : nodeGroup->getNodes()) {
                if (!subnet.contains(node)) continue;
                
                file << "node_" << idx << "[label=\"";
                if (node->getName().length() < 30)
                    file << node->getName();
                else
                    file << "[zip]";
                file << " - " << node->getId() << " - " << node->getTypeName();
                if (auto* reg = dynamic_cast<hlim::Node_Register*>(node)) {
                    file << '[';
                    if (reg->getFlags().containsAnyOf(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD))
                        file << 'F';
                    if (reg->getFlags().containsAnyOf(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD))
                        file << 'B';
                    file << ']';
                }
                file << "\"";
                styleNode(file, node);
                file << "];" << std::endl;
                node2idx[node] = idx;
                idx++;
            }

            file << "}" << std::endl;
        };
        if (nodeGroup == nullptr)
            reccurWalkNodeGroup(circuit.getRootNodeGroup());
        else
            reccurWalkNodeGroup(nodeGroup);

        for (auto &node : circuit.getNodes()) {
            if (node->getGroup() == nullptr) {
                if (!subnet.contains(node.get())) continue;
                file << "node_" << idx << "[label=\"" << node->getName() << " - " << node->getId() << " - " << node->getTypeName();
                if (auto *reg = dynamic_cast<hlim::Node_Register*>(node.get())) {
                    file << '[';
                    if (reg->getFlags().containsAnyOf(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD))
                        file << 'F';
                    if (reg->getFlags().containsAnyOf(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD))
                        file << 'B';
                    file << ']';
                }
                file << "\"";
                styleNode(file, node.get());
                file << "];" << std::endl;
                node2idx[node.get()] = idx;
                idx++;
            }
        }
    }

    for (auto &nodeIdx : node2idx) {
        auto &node = nodeIdx.first;
        unsigned nodeId = nodeIdx.second;

        for (auto port : utils::Range(node->getNumInputPorts())) {
            auto producer = node->getDriver(port);
            if (producer.node == nullptr) continue;

            auto producerIt = node2idx.find(producer.node);
            if (producerIt == node2idx.end()) continue;
            
            unsigned producerId = node2idx.find(producer.node)->second;

            auto type = hlim::getOutputConnectionType(producer);

            file << "node_" << producerId << " -> node_" << nodeId << " [";

            switch (producer.node->getOutputType(producer.port)) {
                case hlim::NodeIO::OUTPUT_LATCHED:
                    file << " style=\"dashed\"";
                break;
                case hlim::NodeIO::OUTPUT_CONSTANT:
                case hlim::NodeIO::OUTPUT_IMMEDIATE:
                default:
                break;
            }


            std::int64_t creationDistance = (std::int64_t)node->getId() - (std::int64_t)producer.node->getId();
            double weight;
            if (creationDistance > 0)
                weight = 100 / std::log(1+std::abs(creationDistance));
            else {
                //file << " constraint=false";
                weight = 1 / std::log(1+std::abs(creationDistance));
            }

            if (producer.node->getGroup() != node->getGroup())
                weight *= 0.01;

            file << " weight="<<std::round(1+weight * 100); // dot wants integers, so scale everything up

            std::string auxLabel;
            if (signalDelays) {
                auto delay = signalDelays->getDelay(producer);

                float maxDelay = 0.0;
                for (auto f : delay)
                    maxDelay = std::max(maxDelay, f);

                auxLabel = std::to_string(maxDelay);

                maxDelay /= 32.0f;
                int red = std::min<int>(255, (int)std::floor(std::max(0.0f, maxDelay - 1.0f) * 255));
                int blue = std::min<int>(255, (int)std::floor(std::max(0.0f, 1.0f-maxDelay) * 255));

                file << (boost::format(" color=\"#%02x00%02x\"") % red % blue);
            }

            file << " label=\"";
            switch (type.interpretation) {
                case hlim::ConnectionType::BOOL:
                    file << "BOOL"; break;
                case hlim::ConnectionType::BITVEC:
                    file << "BVEC(" << type.width << ')'; break;
                case hlim::ConnectionType::DEPENDENCY:
                    file << "DEPENDENCY"; break;
            }

            if (dynamic_cast<hlim::Node_Register*>(node) && port == hlim::Node_Register::Input::RESET_VALUE)
                file << " (reset)";

            if (!auxLabel.empty())
                file << " " << auxLabel;
            file << "\"";

            file << "];" << std::endl;
        }
    }

    file << "}" << std::endl;
}


void DotExport::writeMergedDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet)
{
    struct CombinatoryArea {
        std::set<const hlim::NodeGroup*> nodeGroups;
        std::vector<const hlim::BaseNode*> nodes;
        std::vector<const hlim::Node_Register*> inputRegisters;
        std::vector<const hlim::Node_Register*> outputRegisters;
    };

    std::vector<CombinatoryArea> areas;
    std::map<const hlim::BaseNode*, size_t> node2idx;
    std::vector<const hlim::Node_Register*> registers;

    {
        std::set<const hlim::BaseNode*> handledNodes;
        std::vector<const hlim::BaseNode*> openList;

        for (auto n : subnet) {
            if (auto *reg = dynamic_cast<const hlim::Node_Register*>(n)) {
                registers.push_back(reg);

                continue;
            }
            if (dynamic_cast<const hlim::Node_Signal*>(n)) continue;

            openList.clear();
            openList.push_back(n);

            CombinatoryArea newArea;

            while (!openList.empty()) {
                auto *node = openList.back();
                openList.pop_back();
                if (handledNodes.contains(node)) continue;
                handledNodes.insert(node);

                newArea.nodes.push_back(node);

                for (auto i : utils::Range(node->getNumInputPorts())) {
                    auto driver = node->getDriver(i);
                    if (driver.node ==  nullptr) continue;
                    if (auto *reg = dynamic_cast<const hlim::Node_Register*>(driver.node)) {
                        newArea.inputRegisters.push_back(reg);
                    } else openList.push_back(driver.node);
                }

                for (auto i : utils::Range(node->getNumOutputPorts())) {
                    for (auto driven : node->getDirectlyDriven(i)) {
                        if (auto *reg = dynamic_cast<const hlim::Node_Register*>(driven.node)) {
                            newArea.outputRegisters.push_back(reg);
                        } else openList.push_back(driven.node);
                    }
                }
            }

            if (!newArea.nodes.empty()) {
                areas.push_back(std::move(newArea));
                for (auto n : areas.back().nodes) {
                    node2idx[n] = areas.size()-1;
                    areas.back().nodeGroups.insert(n->getGroup());
                }
            }
        }
    }

    std::fstream file(m_destination.string().c_str(), std::fstream::out);
    if (!file.is_open())
        throw std::runtime_error("Could not open file!");
    file << "digraph G {" << std::endl;

    for (auto idx : utils::Range(areas.size())) {
        file << "node_" << idx << "[label=\"";
        for (auto *g : areas[idx].nodeGroups) {
            file << g->getInstanceName() << " : " << g->getName() << "\\n";
        }
        file << "\"";
        file << " shape=\"box\"";
        file << "];" << std::endl;
    }


    for (auto i : utils::Range(registers.size())) {
        auto *reg = registers[i];
        node2idx[reg] = areas.size()+i;

        auto type = reg->getOutputConnectionType(0);
        file << "node_" << areas.size() + i << "[label=\"";
        file << "Register ";
        switch (type.interpretation) {
            case hlim::ConnectionType::BOOL:
                file << "BOOL"; break;
            case hlim::ConnectionType::BITVEC:
                file << "BVEC(" << type.width << ')'; break;
            case hlim::ConnectionType::DEPENDENCY:
                file << "DEPENDENCY"; break;
        }
        file << "\"";
        file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0a0ff\"";
        file << "];" << std::endl;
    }    


    struct Connection {
        std::string label;
    };

    std::map<std::pair<size_t, size_t>, Connection> connections;

    for (auto i : utils::Range(registers.size())) {
        auto *reg = registers[i];

        for (auto i : utils::Range(reg->getNumInputPorts())) {
            auto driver = reg->getDriver(i);
            if (driver.node ==  nullptr) continue;

            auto it = node2idx.find(driver.node);
            if (it != node2idx.end()) {
                std::pair<size_t, size_t> link = { it->second, node2idx[reg] };
                connections[link].label += driver.node->attemptInferOutputName(driver.port);
            }
        }
        for (auto driven : reg->getDirectlyDriven(0)) {
            auto it = node2idx.find(driven.node);
            if (it != node2idx.end()) {
                std::pair<size_t, size_t> link = { node2idx[reg], it->second };
                auto &con = connections[link];
                if (con.label.empty())
                    con.label = reg->attemptInferOutputName(0);
                else {
                    con.label += "\\n";
                    con.label += reg->attemptInferOutputName(0);
                }
            }
        }
    }


    for (auto &connection : connections) {
        file << "node_" << connection.first.first << " -> node_" << connection.first.second << " [";

        file << " label=\"";
        file << connection.second.label;
        file << "\"";

        file << "];" << std::endl;
    }

    file << "}" << std::endl;
}


void DotExport::runGraphViz(std::filesystem::path destination)
{
    system((std::string("dot -Tsvg ") + m_destination.string() + " -o " + destination.string()).c_str());
}




void visualize(const hlim::Circuit &circuit, const std::string &filename, hlim::NodeGroup *nodeGroup)
{
    DotExport exp(filename+".dot");
    exp(circuit, nodeGroup);
    exp.runGraphViz(filename+".svg");
}

}
